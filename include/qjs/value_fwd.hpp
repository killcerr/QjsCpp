#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "context_fwd.hpp"
#include "conversion_fwd.hpp"
#include "qjs/functionwrapper_fwd.hpp"
#include "qjs/util.hpp"
#include "quickjs.h"
#include "result_fwd.hpp"

namespace Qjs {
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
            void operator = (Value &&value) {
                if constexpr (std::is_same_v<T, std::string>)
                    JS_SetPropertyStr(Parent.ctx, Parent, Index.c_str(), value.ToUnmanaged());
                else
                    JS_SetPropertyInt64(Parent.ctx, Parent, Index, value.ToUnmanaged());
            }

            operator Value () {
                return **this;
            }

            Value operator * () {
                if constexpr (std::is_same_v<T, std::string>)
                    return CreateFree(Parent.ctx, JS_GetPropertyStr(Parent.ctx, Parent, Index.c_str()));
                else
                    return CreateFree(Parent.ctx, JS_GetPropertyInt64(Parent.ctx, Parent, Index));
            }
        };

        Context &ctx;
        JSValue value;

        Value(Context &ctx, JSValue value) : ctx(ctx), value(JS_DupValue(ctx, value)) {}

        Value(Value const &copy) : Value(copy.ctx, copy) {}

        static Value CreateFree(Context &ctx, JSValue val) {
            Value out (ctx, val);
            JS_FreeValue(ctx, val);
            return out;
        }

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
            return CreateFree(ctx, JS_ThrowPlainError(ctx, "%s", message.c_str()));
        }

        static Value ThrowRangeError(Context &ctx, std::string &&message) {
            return CreateFree(ctx, JS_ThrowRangeError(ctx, "%s", message.c_str()));
        }

        static Value ThrowTypeError(Context &ctx, std::string &&message) {
            return CreateFree(ctx, JS_ThrowTypeError(ctx, "%s", message.c_str()));
        }

        static Value Throw(Value err) {
            return CreateFree(err.ctx, JS_Throw(err.ctx, err.ToUnmanaged()));
        }

        static Value Global(Context &ctx) {
            return CreateFree(ctx, JS_GetGlobalObject(ctx));
        }

        static Value Object(Context &ctx) {
            return CreateFree(ctx, JS_NewObject(ctx));
        }

        static Value Array(Context &ctx) {
            return CreateFree(ctx, JS_NewArray(ctx));
        }

        template <typename T>
        JsResult<T> As() {
            if (IsException())
                return *this;

            return Conversion<T>::Unwrap(*this);
        }

        private:
        template <typename ...TArgs, std::size_t... TIndices>
            requires (Conversion<TArgs>::Implemented,...)
        static JsResult<std::tuple<TArgs...>> UnpackArgs(Context &ctx, Value thisVal, int argc, JSValue *argv, std::index_sequence<TIndices...>) {
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

                return TFun(thisVal, values).ToUnmanaged();
            }
        };
        public:

        template <auto TFun>
        static Value Function(Context &ctx, std::string &&name) {
            return CreateFree(ctx, JS_NewCFunction(ctx, FunctionWrapper<TFun>::Invoke, name.c_str(), FunctionWrapper<TFun>::ArgCount));
        }

        template <auto TFun>
            requires requires (Value thisObj, std::vector<Value> params) {
                { TFun(thisObj, params) } -> std::same_as<Value>;
            }
        static Value RawFunction(Context &ctx, std::string &&name) {
            return CreateFree(ctx, JS_NewCFunction(ctx, RawFunctionWrapper<TFun>::Invoke, name.c_str(), FunctionWrapper<TFun>::ArgCount));
        }

        template <auto TGet>
        void AddGetter(Context &ctx, std::string &&name);

        template <auto TGet>
        void AddGetterSetter(Context &ctx, std::string &&name);

        template <typename TReturn, typename ...TArgs>
        JsResult<TReturn> Invoke(TArgs &&...args) {
            std::array<JSValue, sizeof...(TArgs)> argsRaw { Value::From(ctx, std::forward<TArgs>(args)).ToUnmanaged()... };
            Value result = CreateFree(ctx, JS_Call(ctx, value, JS_UNDEFINED, sizeof...(TArgs), argsRaw.data()));
            for (auto &arg : argsRaw)
                JS_FreeValue(ctx, arg);

            if (result.IsException())
                return result;
            
            if constexpr (!std::is_same_v<TReturn, void>)
                return result.As<TReturn>();
            else
                return JsResult<TReturn>();
        }

        template <typename TReturn, typename TThis, typename ...TArgs>
        JsResult<TReturn> InvokeThis(TThis &&_this, TArgs &&...args) {
            std::array<JSValue, sizeof...(TArgs)> argsRaw { Value::From(ctx, std::forward<TArgs>(args)).ToUnmanaged()... };
            Value result = CreateFree(ctx, JS_Call(ctx, value, Conversion<TThis>::Wrap(_this), sizeof...(TArgs), argsRaw.data()));
            for (auto &arg : argsRaw)
                JS_FreeValue(ctx, arg);

            if (result.IsException())
                return result;
            
            if constexpr (!std::is_same_v<TReturn, void>)
                return result.As<TReturn>();
            else
                return JsResult<TReturn>();
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

        bool IsNullish() const {
            return value.tag == JS_TAG_NULL || value.tag == JS_TAG_UNDEFINED || value.tag == JS_TAG_UNINITIALIZED;
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
            return CreateFree(ctx, JS_GetException(ctx)).ToString().OkOr("Unknown error.");
        }

        Value Await() {
            while (1) {
                JSPromiseStateEnum state = JS_PromiseState(ctx, value);
                switch (state) {
                    case JS_PROMISE_FULFILLED: {
                        auto val = CreateFree(ctx, JS_PromiseResult(ctx, value));

                        return val;
                    }
                    case JS_PROMISE_REJECTED: {
                        auto val = CreateFree(ctx, JS_PromiseResult(ctx, value));

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
            Value proto = CreateFree(ctx, JS_GetProperty(ctx, value, JS_ATOM_prototype));

            return proto;
        }

        /// Creates an unmanaged value. It's up to you to manage the lifetime.
        JSValue ToUnmanaged() const {
            return JS_DupValue(ctx, value);
        }
    };
}
