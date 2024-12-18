#pragma once

#include "conversion_fwd.hpp"
#include "qjs/result_fwd.hpp"
#include "value_fwd.hpp"
#include "qjs/classwrapper_fwd.hpp"
#include "functionwrapper_fwd.hpp"
#include <cstddef>
#include <format>
#include <tuple>
#include <type_traits>

namespace Qjs {
    template <typename ...TArgs, std::size_t... TIndices>
        requires (Conversion<TArgs>::Implemented,...)
    static JsResult<std::tuple<TArgs...>> UnpackArgsImpl(Context &ctx, Value thisVal, int argc, JSValue *argv, std::index_sequence<TIndices...>) {
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

    template <typename ...TArgs>
        requires (Conversion<TArgs>::Implemented,...)
    struct UnpackWrapper<TArgs...> {
        static JsResult<std::tuple<TArgs...>> UnpackArgs(Context &ctx, Value thisVal, int argc, JSValue *argv) {
            return UnpackArgsImpl<TArgs...>(ctx, thisVal, argc, argv, std::make_index_sequence<sizeof...(TArgs)>());
        }
    };

    template <>
    struct UnpackWrapper<> {
        static JsResult<std::tuple<>> UnpackArgs(Context &ctx, Value thisVal, int argc, JSValue *argv) {
            return std::tuple<>();
        }
    };

    template <>
    struct UnpackWrapper<Value> {
        static JsResult<std::tuple<Value>> UnpackArgs(Context &ctx, Value thisVal, int argc, JSValue *argv) {
            return std::tuple<Value>(thisVal);
        }
    };

    template <typename ...TArgs>
        requires ((Conversion<TArgs>::Implemented,...))
    struct UnpackWrapper<Value, TArgs...> {
        static JsResult<std::tuple<Value, TArgs...>> UnpackArgs(Context &ctx, Value thisVal, int argc, JSValue *argv) {
            auto result = UnpackArgsImpl<TArgs...>(ctx, thisVal, argc, argv, std::make_index_sequence<sizeof...(TArgs)>());
            if (!result.IsOk())
                return result.GetErr();

            return std::tuple_cat(std::tuple<Value> {thisVal}, result.GetOk());
        }
    };

    template <typename TReturn, typename ...TArgs, TReturn (*TFun)(TArgs...)>
    struct FunctionWrapper<TFun> {
        static constexpr size_t ArgCount = sizeof...(TArgs);

        static JSValue Invoke(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");
            auto &ctx = *_ctx;

            Value thisVal {ctx, this_val};
            
            JsResult<std::tuple<std::decay_t<TArgs>...>> optArgs = UnpackWrapper<std::decay_t<TArgs>...>::UnpackArgs(ctx, thisVal, argc, argv);

            if (!optArgs.IsOk())
                return optArgs.GetErr().ToUnmanaged();

            std::tuple<std::decay_t<TArgs>...> args = optArgs.GetOk();

            if constexpr (std::is_same_v<TReturn, void>) {
                std::apply(TFun, args);
                return Value::Undefined(ctx).ToUnmanaged();
            } else {
                return Value::From(ctx, std::apply(TFun, args)).ToUnmanaged();
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
            
            JsResult<std::tuple<std::decay_t<TArgs>...>> optArgs = UnpackWrapper<std::decay_t<TArgs>...>::UnpackArgs(ctx, thisVal, argc, argv);

            if (!optArgs.IsOk())
                return optArgs.GetErr().ToUnmanaged();

            std::tuple<std::decay_t<TArgs>...> args = optArgs.GetOk();

            if constexpr (std::is_same_v<TReturn, void>) {
                std::apply(TFun, args);
                return Value::Undefined(ctx).ToUnmanaged();
            } else {
                return Value::From(ctx, std::apply(TFun, args)).ToUnmanaged();
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
            
            JsResult<std::tuple<std::decay_t<TArgs>...>> optArgs = UnpackWrapper<std::decay_t<TArgs>...>::UnpackArgs(ctx, thisVal, argc, argv);

            if (!optArgs.IsOk())
                return optArgs.GetErr().ToUnmanaged();

            std::tuple<std::decay_t<TArgs>...> args = optArgs.GetOk();

            auto thisRes = thisVal.As<RequireNonNull<TThis>>();

            if (!thisRes.IsOk())
                return thisRes.GetErr().ToUnmanaged();

            TThis *_this = thisRes.GetOk();

            if constexpr (std::is_same_v<TReturn, void>) {
                std::apply(TFun, std::tuple_cat(std::tuple<TThis *>(_this), args));
                return Value::Undefined(ctx).ToUnmanaged();
            } else {
                return Value::From(ctx, std::apply(TFun, std::tuple_cat(std::tuple<TThis *>(_this), args))).ToUnmanaged();
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
            
            JsResult<std::tuple<std::decay_t<TArgs>...>> optArgs = UnpackWrapper<std::decay_t<TArgs>...>::UnpackArgs(ctx, thisVal, argc, argv);

            if (!optArgs.IsOk())
                return optArgs.GetErr().ToUnmanaged();

            std::tuple<std::decay_t<TArgs>...> args = optArgs.GetOk();

            auto thisRes = thisVal.As<RequireNonNull<TThis>>();
            
            if (!thisRes.IsOk())
                return thisRes.GetErr().ToUnmanaged();

            TThis *_this = thisRes.GetOk();

            if constexpr (std::is_same_v<TReturn, void>) {
                std::apply(TFun, std::tuple_cat(std::tuple<TThis *>(_this), args));
                return Value::Undefined(ctx).ToUnmanaged();
            } else {
                return Value::From(ctx, std::apply(TFun, std::tuple_cat(std::tuple<TThis *>(_this), args))).ToUnmanaged();
            }
        }
    };

    template <typename TClass, typename TValue, TValue (TClass::*TGetSet)>
    struct GetSetWrapper<TGetSet> {
        static JSValue Get(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");
            auto &ctx = *_ctx;

            Value thisVal {ctx, this_val};

            TClass *t = ClassWrapper<TClass>::Get(thisVal);
            if (!t)
                return Value::ThrowTypeError(ctx, std::format("Expected type {}.", NameOf<TClass>())).ToUnmanaged();

            return Value::From(ctx, t->*TGetSet).ToUnmanaged();
        }

        template <typename = void>
            requires (!std::is_const_v<TValue>)
        static JSValue Set(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");
            auto &ctx = *_ctx;

            Value thisVal {ctx, this_val};

            TClass *t = ClassWrapper<TClass>::Get(thisVal);
            if (!t)
                return Value::ThrowTypeError(ctx, std::format("Expected type {}.", NameOf<TClass>())).ToUnmanaged();

            Value set {ctx, JS_UNDEFINED};
            if (argc != 0)
                set = Value(ctx, argv[0]);

            auto res = set.As<TValue>();
            if (!res.IsOk())
                return res.GetErr().ToUnmanaged();

            t->*TGetSet = res.GetOk();

            return Value::From(ctx, t->*TGetSet).ToUnmanaged();
        }
    };
}
