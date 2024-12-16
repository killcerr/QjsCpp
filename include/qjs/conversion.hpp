#pragma once

#include "qjs/class.hpp"
#include "qjs/classwrapper_fwd.hpp"
#include "qjs/context_fwd.hpp"
#include "quickjs.h"
#include "value_fwd.hpp"

#include "conversion_fwd.hpp"
#include <expected>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Qjs {
    template <typename TInt>
        requires std::is_integral_v<TInt>
    struct Conversion<TInt> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, TInt value) {
            return Value(ctx, JS_NewInt64(ctx, value));
        }

        static JsResult<TInt> Unwrap(Value const &value) {
            if (!ValidValue(value))
                return Value::ThrowTypeError(value.ctx, "Expected number");

            if (value.value.tag == JS_TAG_INT)
                return TInt(value.value.u.int32);
            
            return TInt(value.value.u.float64);
        }

        static bool ValidValue(Value const &value) {
            return JS_IsNumber(value);
        }
    };

    template <typename TFloat>
        requires std::is_floating_point_v<TFloat>
    struct Conversion<TFloat> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, TFloat value) {
            return Value(ctx, JS_NewFloat64(ctx, int64_t(value)));
        }

        static JsResult<TFloat> Unwrap(Value const &value) {
            if (!ValidValue(value))
                return Value::ThrowTypeError(value.ctx, "Expected number");

            if (value.value.tag == JS_TAG_INT)
                return TFloat(value.value.u.int32);
            
            return TFloat(value.value.u.float64);
        }

        static bool ValidValue(Value const &value) {
            return JS_IsNumber(value);
        }
    };

    template <typename T>
        requires Conversion<T>::Implemented
    struct Conversion<std::optional<T>> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, std::optional<T> const &value) {
            if (!value)
                return Value::Null(ctx);

            return Conversion<T>::Wrap(ctx, *value);
        }

        static JsResult<std::optional<T>> Unwrap(Value const &value) {
            auto tag = value.value.tag;
            if (tag == JS_TAG_UNDEFINED || tag == JS_TAG_NULL || tag == JS_TAG_UNINITIALIZED)
                return std::nullopt;

            return Conversion<T>::Unwrap(value);
        }
    };

    template <>
    struct Conversion<bool> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, bool value) {
            return Value(ctx, JS_NewBool(ctx, value));
        }

        static JsResult<bool> Unwrap(Value const &value) {
            return JS_ToBool(value.ctx, value);
        }
    };

    template <>
    struct Conversion<std::string> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, std::string const &value) {
            return Value(ctx,  JS_NewStringLen(ctx, value.c_str(), value.size()));
        }

        static JsResult<std::string> Unwrap(Value const &value) {
            return value.ToString();
        }
    };

    template <typename TReturn, typename ...TArgs>
    struct Conversion<std::function<TReturn(TArgs...)>> final {
        static constexpr bool Implemented = true;

        using TFun = std::function<TReturn(TArgs...)>;
        using Wrapper = ClassWrapper<Conversion<TFun>>;
        std::function<TReturn(TArgs...)> fun;

        static JSValue Invoke(JSContext *__ctx, JSValue func_obj, JSValue this_val, int argc, JSValue *argv, int flags) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");

            auto &ctx = *_ctx;
            Value funcObj {ctx, func_obj};

            Conversion *conv = Wrapper::Get(funcObj);

            auto args = Value::UnpackArgs<TArgs...>(ctx, argc, argv);
            if (!args.IsOk())
                return JS_DupValue(ctx, args.GetErr());
            
            if constexpr (std::is_same_v<TReturn, void>) {
                std::apply(std::forward<TFun>(conv->fun), args.GetOk());
                return JS_UNDEFINED;
            } else {
                auto result = Value::From(ctx, std::apply(std::forward<TFun>(conv->fun), args.GetOk()));
                return JS_DupValue(ctx, result);
            }
        }

        static Value Wrap(Context &ctx, TFun f) {
            Wrapper::RegisterClass(ctx, "Function", Invoke);

            return Wrapper::New(ctx, new Conversion {f});
        }

        static JsResult<TFun> Unwrap(Value value) {
            Wrapper::RegisterClass(value.ctx, "Function", Invoke);

            if (JS_GetClassID(value) == Wrapper::GetClassId(value.ctx.rt))
                return Wrapper::Get(value)->fun;

            return (TFun) [&](TArgs ...args) -> TReturn {
                return value.ToFunction<TReturn, TArgs...>()(args...).GetOk();
            };
        }
    };

    template <typename T>
        requires std::is_base_of_v<Class, T>
    struct Conversion<T *> final {
        static constexpr bool Implemented = true;
        using Wrapper = ClassWrapper<T>;

        static Value Wrap(Context &ctx, T *cl) {
            return Wrapper::New(ctx, cl);
        }

        static T *Unwrap(Value value) {
            T *out = Wrapper::Get(value);;
            out->jsThis = value;
            return out;
        }
    };
    

    template <typename T>
        requires (std::is_base_of_v<Class, T> && std::is_copy_constructible_v<T>)
    struct Conversion<T> final {
        static constexpr bool Implemented = true;
        using Wrapper = ClassWrapper<T>;

        static Value Wrap(Context &ctx, T const &cl) {
            return Wrapper::New(ctx, new T(cl));
        }

        static T Unwrap(Value value) {
            T *out = Wrapper::Get(value);;
            out->jsThis = value;
            return *out;
        }
    };
}
