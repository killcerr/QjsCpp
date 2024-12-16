#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "context_fwd.hpp"
#include "conversion_fwd.hpp"
#include "qjs/util.hpp"
#include "quickjs.h"

namespace Qjs {
    struct Value;

    template <typename T>
    struct JsResult final {
        private:
        std::variant<T, Value> const Values;

        public:
        JsResult(T ok) : Values({ok}) {}
        JsResult(Value &&err) : Values({err}) {}
        JsResult(Value const &err) : Values({err}) {}

        bool IsOk() {
            return Values.index() == 0;
        }

        T GetOk() noexcept(false) {
            if (!IsOk())
                throw GetErr();
            return std::get<0>(Values);
        }

        T OkOr(T &&other) {
            if (IsOk())
                return GetOk();
            return other;
        }

        Value GetErr();
    };

    struct Value final {
        template <typename T>
            requires (std::is_same_v<T, std::string> || std::is_same_v<T, size_t>)
        struct PropertyReference {
            private:
            Value const &Parent;
            T const Index;

            PropertyReference(Value &parent, T index) : Parent(parent), Index(index) {}
            friend struct Value;

            public:
            void operator = (Value const &value) {
                if constexpr (std::is_same_v<T, std::string>)
                    JS_SetPropertyStr(Parent.ctx, Parent, Index.c_str(), value);
                else
                    JS_SetPropertyInt64(Parent.ctx, Parent, Index, value);
            }

            operator Value () {
                return **this;
            }

            Value operator * () {
                if constexpr (std::is_same_v<T, std::string>)
                    return Value(Parent.ctx, JS_GetPropertyStr(Parent.ctx, Parent, Index.c_str()));
                else
                    return Value(Parent.ctx, JS_GetPropertyInt64(Parent.ctx, Parent, Index));
            }
        };

        Context &ctx;
        JSValue value;

        Value(Context &ctx, JSValue value) : ctx(ctx), value(JS_DupValue(ctx, value)) {}

        Value(Value const &copy) : Value(copy.ctx, copy) {}

        void operator = (Value const &copy) {
            this->~Value();
            new (this) Value(copy);
        }

        ~Value() {
            JS_FreeValue(ctx, value);
        }
        
        template <typename T>
            requires Conversion<T>::Implemented
        Value(Context &ctx, T &&value) : Value(Conversion<T>::Wrap(ctx, std::forward<T>(value))) {}

        template <typename T>
        static Value From(Context &ctx, T const &value) {
            return Conversion<T>::Wrap(ctx, value);
        }

        static Value Null(Context &ctx) {
            return Value(ctx, JS_NULL);
        }

        static Value Undefined(Context &ctx) {
            return Value(ctx, JS_UNDEFINED);
        }

        static Value ThrowPlainError(Context &ctx, std::string &&message) {
            return Value(ctx, JS_ThrowPlainError(ctx, "%s", message.c_str()));
        }

        static Value ThrowRangeError(Context &ctx, std::string &&message) {
            return Value(ctx, JS_ThrowRangeError(ctx, "%s", message.c_str()));
        }

        static Value ThrowTypeError(Context &ctx, std::string &&message) {
            return Value(ctx, JS_ThrowTypeError(ctx, "%s", message.c_str()));
        }

        static Value Throw(Value err) {
            return Value(err.ctx, JS_Throw(err.ctx, JS_DupValue(err.ctx, err)));
        }

        static Value Global(Context &ctx) {
            auto obj = JS_GetGlobalObject(ctx);
            Value val {ctx, obj};
            JS_FreeValue(ctx, obj);
            return val;
        }

        static Value Object(Context &ctx) {
            auto obj = JS_NewObject(ctx);
            Value val {ctx, obj};
            JS_FreeValue(ctx, obj);
            return val;
        }

        static Value Array(Context &ctx) {
            auto arr = JS_NewArray(ctx);
            Value val {ctx, arr};
            JS_FreeValue(ctx, arr);
            return val;
        }

        template <typename T>
        JsResult<T> As() {
            if (IsException())
                return *this;

            return Conversion<T>::Unwrap(*this);
        }

        private:
        template <typename ...TArgs, std::size_t... TIndices>
        static JsResult<std::tuple<TArgs...>> UnpackArgs(Context &ctx, int argc, JSValue *argv, std::index_sequence<TIndices...>) {
            std::array<Value, sizeof...(TArgs)> rawArgs {(Unit<TArgs>(Value::Undefined(ctx)))...};
            for (size_t i = 0; i < std::min(sizeof...(TArgs), size_t(argc)); i++)
                rawArgs[i] = Value(ctx, argv[i]);
            
            try {
                std::tuple<TArgs...> args {rawArgs[TIndices].template As<TArgs>().GetOk()...};
                return args;
            } catch (Value v) {
                return v;
            }
        }

        public:
        template <typename ...TArgs>
        static JsResult<std::tuple<TArgs...>> UnpackArgs(Context &ctx, int argc, JSValue *argv) {
            return UnpackArgs<TArgs...>(ctx, argc, argv, std::make_index_sequence<sizeof...(TArgs)>());
        }

        private:
        template <auto TFun>
        struct FunctionWrapper;

        template <typename TReturn, typename ...TArgs, TReturn (*TFun)(TArgs...)>
        struct FunctionWrapper<TFun> {
            static constexpr size_t ArgCount = sizeof...(TArgs);

            static JSValue Invoke(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
                auto _ctx = Context::From(__ctx);
                if (!_ctx)
                    return JS_ThrowPlainError(__ctx, "Whar");
                auto &ctx = *_ctx;

                Value thisVal {ctx, this_val};
                
                JsResult<std::tuple<TArgs...>> optArgs = UnpackArgs<TArgs...>(ctx, argc, argv);

                if (!optArgs.IsOk())
                    return JS_DupValue(ctx, optArgs.GetErr());

                std::tuple<TArgs...> args = optArgs.GetOk();

                if constexpr (std::is_same_v<TReturn, void>) {
                    std::apply(TFun, args);
                    return JS_DupValue(ctx, Value::Undefined(ctx));
                } else {
                    return JS_DupValue(ctx, Value::From(ctx, std::apply(TFun, args)));
                }
            }
        };

        template <typename TReturn, typename ...TArgs, std::function<TReturn (TArgs...)> TFun>
        struct FunctionWrapper<TFun> {
            static constexpr size_t ArgCount = sizeof...(TArgs);

            static JSValue Invoke(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
                auto _ctx = Context::From(__ctx);
                if (!_ctx)
                    return JS_ThrowPlainError(__ctx, "Whar");
                auto &ctx = *_ctx;

                Value thisVal {ctx, this_val};
                
                JsResult<std::tuple<TArgs...>> optArgs = UnpackArgs<TArgs...>(ctx, argc, argv);

                if (!optArgs.IsOk())
                    return JS_DupValue(ctx, optArgs.GetErr());

                std::tuple<TArgs...> args = optArgs.GetOk();

                if constexpr (std::is_same_v<TReturn, void>) {
                    std::apply(TFun, args);
                    return JS_DupValue(ctx, Value::Undefined(ctx));
                } else {
                    return JS_DupValue(ctx, Value::From(ctx, std::apply(TFun, args)));
                }
            }
        };

        template <typename TReturn, typename TThis, typename ...TArgs, TReturn (TThis::*TFun)(TArgs...)>
        struct FunctionWrapper<TFun> {
            static constexpr size_t ArgCount = sizeof...(TArgs);

            static JSValue Invoke(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
                auto _ctx = Context::From(__ctx);
                if (!_ctx)
                    return JS_ThrowPlainError(__ctx, "Whar");
                auto &ctx = *_ctx;

                Value thisVal {ctx, this_val};
                
                JsResult<std::tuple<TArgs...>> optArgs = UnpackArgs<TArgs...>(ctx, argc, argv);

                if (!optArgs.IsOk())
                    return JS_DupValue(ctx, optArgs.GetErr());

                std::tuple<TArgs...> args = optArgs.GetOk();

                auto thisRes = thisVal.As<TThis *>();;
                if (!thisRes.IsOk())
                    return JS_DupValue(ctx, thisRes.GetErr());

                TThis *_this = thisRes.GetOk();

                if constexpr (std::is_same_v<TReturn, void>) {
                    std::apply(TFun, std::tuple_cat(std::tuple<TThis *>(_this), args));
                    return JS_DupValue(ctx, Value::Undefined(ctx));
                } else {
                    return JS_DupValue(ctx, Value::From(ctx, std::apply(TFun, std::tuple_cat(std::tuple<TThis *>(_this), args))));
                }
            }
        };

        template <typename TReturn, typename TThis, typename ...TArgs, TReturn (TThis::*TFun)(TArgs...) const>
        struct FunctionWrapper<TFun> {
            static constexpr size_t ArgCount = sizeof...(TArgs);

            static JSValue Invoke(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
                auto _ctx = Context::From(__ctx);
                if (!_ctx)
                    return JS_ThrowPlainError(__ctx, "Whar");
                auto &ctx = *_ctx;

                Value thisVal {ctx, this_val};
                
                JsResult<std::tuple<TArgs...>> optArgs = UnpackArgs<TArgs...>(ctx, argc, argv);

                if (!optArgs.IsOk())
                    return JS_DupValue(ctx, optArgs.GetErr());

                std::tuple<TArgs...> args = optArgs.GetOk();

                auto thisRes = thisVal.As<TThis *>();;
                if (!thisRes.IsOk())
                    return JS_DupValue(ctx, thisRes.GetErr());

                TThis *_this = thisRes.GetOk();

                if constexpr (std::is_same_v<TReturn, void>) {
                    std::apply(TFun, std::tuple_cat(std::tuple<TThis *>(_this), args));
                    return JS_DupValue(ctx, Value::Undefined(ctx));
                } else {
                    return JS_DupValue(ctx, Value::From(ctx, std::apply(TFun, std::tuple_cat(std::tuple<TThis *>(_this), args))));
                }
            }
        };

        template <auto TGetSet>
        struct GetSetWrapper;

        template <auto TFun>
            requires requires (Value thisObj, std::vector<Value> params) {
                { TFun(thisObj, params) } -> std::same_as<Value>;
            }
        struct RawFunctionWrapper {
            static JSValue Invoke(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
                auto _ctx = Context::From(__ctx);
                if (!_ctx)
                    return JS_ThrowPlainError(__ctx, "Whar");
                auto &ctx = *_ctx;

                Value thisVal {ctx, this_val};
                
                std::vector<Value> values {size_t(argc), Value::Undefined(ctx)};

                for (size_t i = 0; i < values.size(); i++)
                    values[i] = Value(ctx, argv[i]);

                return JS_DupValue(ctx, TFun(thisVal, values));
            }
        };
        public:

        template <auto TFun>
        static Value Function(Context &ctx, std::string &&name) {
            return Value(ctx, JS_NewCFunction(ctx, FunctionWrapper<TFun>::Invoke, name.c_str(), FunctionWrapper<TFun>::ArgCount));
        }

        template <auto TFun>
            requires requires (Value thisObj, std::vector<Value> params) {
                { TFun(thisObj, params) } -> std::same_as<Value>;
            }
        static Value RawFunction(Context &ctx, std::string &&name) {
            return Value(ctx, JS_NewCFunction(ctx, RawFunctionWrapper<TFun>::Invoke, name.c_str(), FunctionWrapper<TFun>::ArgCount));
        }

        template <auto TGet>
        void AddGetter(Context &ctx, std::string &&name);

        template <auto TGet>
        void AddGetterSetter(Context &ctx, std::string &&name);

        template <typename TReturn, typename ...TArgs>
        std::function<JsResult<TReturn> (TArgs...)> ToFunction() {
            return [&](TArgs ...args) -> JsResult<TReturn> {
                std::array<JSValue, sizeof...(TArgs)> argsRaw { JS_DupValue(ctx, Value::From(ctx, std::forward<TArgs>(args)))... };
                auto result = Value(ctx, JS_Call(ctx, value, JS_UNDEFINED, sizeof...(TArgs), argsRaw.data()));
                for (auto &arg : argsRaw)
                    JS_FreeValue(ctx, arg);

                if (result.IsException())
                    return result;
                
                if constexpr (!std::is_same_v<TReturn, void>)
                    return result.As<TReturn>();
            };
        }

        template <typename TReturn, typename TThis, typename ...TArgs>
        std::function<JsResult<TReturn> (TThis, TArgs...)> ToThisFunction() {
            return [&](TThis _this, TArgs ...args) -> JsResult<TReturn> {
                std::array<JSValue, sizeof...(TArgs)> argsRaw { JS_DupValue(ctx, Value::From(ctx, std::forward<TArgs>(args)))... };
                Value thisObj = Value::From(ctx, std::forward<TThis>(_this));
                auto result = Value(ctx, JS_Call(ctx, value, thisObj, sizeof...(TArgs), argsRaw.data()));
                for (auto &arg : argsRaw)
                    JS_FreeValue(ctx, arg);

                if (result.IsException())
                    return result;
                
                if constexpr (!std::is_same_v<TReturn, void>)
                    return result.As<TReturn>();
            };
        }

        operator JSValue () const {
            return value;
        }

        template <typename T>
            requires (std::is_convertible_v<T, std::string> || std::is_convertible_v<T, size_t>)
        auto operator [] (T &&index) {
            if constexpr (std::is_convertible_v<T, std::string>)
                return PropertyReference<std::string>(*this, index);
            else
                return PropertyReference<size_t>(*this, index);
        }

        bool IsException() {
            return JS_IsException(value);
        }

        JsResult<std::string> ToString() const {
            auto cstr = JS_ToCString(ctx, value);

            if (!cstr)
                return Value(ctx, JS_EXCEPTION);

            std::string str = cstr;

            JS_FreeCString(ctx, cstr);

            return str;
        }

        template <typename = void>
        std::string ExceptionMessage() {
            Value exception {ctx, JS_GetException(ctx)};
            return exception.ToString().OkOr("Unknown error.");
        }

        Value Await() {
            while (1) {
                JSPromiseStateEnum state = JS_PromiseState(ctx, value);
                switch (state) {
                    case JS_PROMISE_FULFILLED: {
                        auto res = JS_PromiseResult(ctx, value);

                        auto val = Value(ctx, res);

                        JS_FreeValue(ctx, res);

                        return val;
                    }
                    case JS_PROMISE_REJECTED: {
                        auto res = JS_PromiseResult(ctx, value);

                        auto val = Value(ctx, res);

                        JS_FreeValue(ctx, res);

                        return Throw(val);
                    }
                    case JS_PROMISE_PENDING:
                        ctx.ExecutePendingJob();
                    default:
                        return *this;
                }
            }
        }

        Value Prototype() {
            static const JSAtom JS_ATOM_prototype = JS_NewAtom(ctx, "prototype");
            auto val = JS_GetProperty(ctx, value, JS_ATOM_prototype);
            Value proto {ctx, val};
            JS_FreeValue(ctx, val);

            return proto;
        }
    };

    template <typename T>
    Value JsResult<T>::GetErr() {
        return std::get<1>(Values);
    }
}
