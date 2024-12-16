#pragma once

#include "value.hpp"

#include "conversion_fwd.hpp"
#include <expected>

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
            return Value(JS_NewFloat64(ctx, int64_t(value)));
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
}
