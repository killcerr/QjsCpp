#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "context.hpp"
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

        T GetOk() {
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
        static Value From(Context &ctx, T value) {
            return Conversion<T>::Wrap(ctx, value);
        }

        static Value Null(Context &ctx) {
            return Value(ctx, JS_NULL);
        }

        static Value Undefined(Context &ctx) {
            return Value(ctx, JS_UNDEFINED);
        }

        static Value ThrowTypeError(Context &ctx, std::string &&message) {
            return Value(ctx, JS_ThrowTypeError(ctx, "%s", message.c_str()));
        }

        static Value Throw(Context &ctx, Value err) {
            return Value(ctx, JS_Throw(ctx, JS_DupValue(ctx, err)));
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

                if (!optArgs.IsOk()) {
                    auto err = optArgs.GetErr();
                    return JS_DupValue(ctx, err);
                }

                std::tuple<TArgs...> args = optArgs.GetOk();

                return Value::From(ctx, std::apply(TFun, args));
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

                if (!optArgs.IsOk()) {
                    auto err = optArgs.GetErr();
                    return JS_DupValue(ctx, err);
                }

                std::tuple<TArgs...> args = optArgs.GetOk();

                return Value::From(ctx, std::apply(TFun, args));
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

                if (!optArgs.IsOk()) {
                    auto err = optArgs.GetErr();
                    return JS_DupValue(ctx, err);
                }

                std::tuple<TArgs...> args = optArgs.GetOk();

                return Value::From(ctx, std::apply(TFun, std::tuple_cat({thisVal.As<TThis>()}, args)));
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

                if (!optArgs.IsOk()) {
                    auto err = optArgs.GetErr();
                    return JS_DupValue(ctx, err);
                }

                std::tuple<TArgs...> args = optArgs.GetOk();

                return Value::From(ctx, std::apply(TFun, std::tuple_cat({thisVal.As<TThis>()}, args)));
            }
        };
        public:

        template <auto TFun>
        static Value Function(Context &ctx, std::string &&name) {
            return Value(ctx, JS_NewCFunction(ctx, FunctionWrapper<TFun>::Invoke, name.c_str(), FunctionWrapper<TFun>::ArgCount));
        }

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
                    case JS_PROMISE_FULFILLED:
                        return Value(ctx, JS_PromiseResult(ctx, value));
                    case JS_PROMISE_REJECTED:
                        return Throw(ctx, Value(ctx, JS_PromiseResult(ctx, value)));
                    case JS_PROMISE_PENDING:
                        ctx.ExecutePendingJob();
                    default:
                        return *this;
                }
            }
        }
    };

    template <typename T>
    Value JsResult<T>::GetErr() {
        return std::get<1>(Values);
    }
}
