#pragma once

#include "qjs/classwrapper_fwd.hpp"
#include "qjs/context_fwd.hpp"
#include "qjs/function.hpp"
#include "qjs/functionwrapper_fwd.hpp"
#include "qjs/result_fwd.hpp"
#include "qjs/util.hpp"
#include "quickjs.h"
#include "value_fwd.hpp"
#include "qjs/class.hpp"

#include "conversion_fwd.hpp"
#include <cassert>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Qjs {
    template <typename T>
    struct RequireNonNull final {
        T *const Ptr;

        RequireNonNull(T *value) : Ptr(value) {
            assert(Ptr);
        }
        RequireNonNull(RequireNonNull const &copy) : Ptr(copy.Ptr) {}

        T operator * () const {
            return *Ptr;
        }

        operator T * () const {
            return Ptr;
        }

        T *operator -> () const {
            return Ptr;
        }

        operator bool () const {
            return Ptr;
        }
    };


    template <typename T>
        requires Conversion<T>::Implemented
    struct PassJsThis<T> final {
        Value jsThis;
        T value;

        operator T () const {
            return value;
        }

        T operator * () const {
            return value;
        }

        T *operator -> () const {
            return &value;
        }
    };

    template <typename TInt>
        requires std::is_integral_v<TInt>
    struct Conversion<TInt> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, TInt value) {
            return Value::CreateFree(ctx, JS_NewInt64(ctx, value));
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
            return Value::CreateFree(ctx, JS_NewFloat64(ctx, int64_t(value)));
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

        static JsResult<std::optional<T>> Unwrap(Value value) {
            auto tag = value.value.tag;
            if (value.IsNullish())
                return std::optional<T>(std::nullopt);

            auto val = Conversion<T>::Unwrap(value);

            if (!val.IsOk())
                return val.GetErr();

            return std::optional<T>(val.GetOk());
        }
    };

    template <>
    struct Conversion<bool> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, bool value) {
            return Value::CreateFree(ctx, JS_NewBool(ctx, value));
        }

        static JsResult<bool> Unwrap(Value const &value) {
            return JS_ToBool(value.ctx, value);
        }
    };

    template <>
    struct Conversion<std::string> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, std::string const &value) {
            return Value::CreateFree(ctx, JS_NewStringLen(ctx, value.c_str(), value.size()));
        }

        static JsResult<std::string> Unwrap(Value const &value) {
            return value.ToString();
        }
    };

    template <typename TReturn, typename ...TArgs>
        requires (Conversion<TReturn>::Implemented || std::is_same_v<TReturn, void>)
    struct Conversion<std::function<TReturn(TArgs...)>> final {
        static constexpr bool Implemented = true;

        using TFun = std::function<TReturn(TArgs...)>;
        using Wrapper = ClassWrapper<Conversion<TFun>>;
        TFun fun;

        static JSValue Invoke(JSContext *__ctx, JSValue func_obj, JSValue this_val, int argc, JSValue *argv, int flags) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");

            auto &ctx = *_ctx;
            Value funcObj {ctx, func_obj};

            Conversion *conv = Wrapper::Get(funcObj);

            Value thisVal = Value(ctx, this_val);

            auto args = UnpackWrapper<std::decay_t<TArgs>...>::UnpackArgs(ctx, thisVal, argc, argv);
            if (!args.IsOk())
                return args.GetErr().ToUnmanaged();
            
            if constexpr (std::is_same_v<TReturn, void>) {
                std::apply(std::forward<TFun>(conv->fun), args.GetOk());
                return JS_UNDEFINED;
            } else {
                return Value::From(ctx, std::apply(std::forward<TFun>(conv->fun), args.GetOk())).ToUnmanaged();
            }
        }

        static Value Wrap(Context &ctx, TFun f) {
            Wrapper::RegisterClass(ctx, "Function", Invoke);

            return Wrapper::New(ctx, new Conversion {f});
        }

        static JsResult<TFun> Unwrap(Value const &value) {
            Wrapper::RegisterClass(value.ctx, "Function", Invoke);

            if (JS_GetClassID(value) == Wrapper::GetClassId(value.ctx.rt))
                return Wrapper::Get(value)->fun;

            return (TFun) [&](TArgs ...args) -> TReturn {
                if constexpr (std::is_same_v<TReturn, void>)
                    value.Invoke<TReturn, TArgs...>(args...);
                else value.Invoke<TReturn, TArgs...>(args...).GetOk();
            };
        }
    };
    
    template <typename TReturn, typename ...TArgs>
    struct Conversion<Function<TReturn, TArgs...>> final {
        static constexpr bool Implemented = true;

        using Func = Function<TReturn, TArgs...>;

        static Value Wrap(Context &ctx, Func f) {
            return f.value;
        }

        static JsResult<Func> Unwrap(Value &value) {
            if (!JS_IsFunction(value.ctx, value))
                return Value::ThrowTypeError(value.ctx, "Expected function.");
            return Func {value};
        }
    };

    template <typename T>
        requires std::is_base_of_v<ManagedClass, T>
    struct Conversion<T *> final {
        static constexpr bool Implemented = true;
        using Wrapper = ClassWrapper<T>;

        static Value Wrap(Context &ctx, T *cl) {
            if (cl == nullptr)
                return Value::Null(ctx);

            return Wrapper::New(ctx, cl);
        }

        static JsResult<T *> Unwrap(Value const &value) {
            if (value.IsNullish())
                return nullptr;

            if (!Wrapper::IsThis(value))
                return Value::ThrowTypeError(value.ctx, std::format("Expected {}", NameOf<T>()));

            T *out = Wrapper::Get(value);
            return out;
        }
    };

    template <typename T>
        requires std::is_base_of_v<UnmanagedClass, T>
    struct Conversion<T *> final {
        static constexpr bool Implemented = true;
        using Wrapper = ClassWrapper<T>;

        static Value Wrap(Context &ctx, T *cl) {
            if (cl == nullptr)
                return Value::Null(ctx);

            return Wrapper::New(ctx, cl);
        }

        static JsResult<T *> Unwrap(Value const &value) {
            if (value.IsNullish())
                return nullptr;

            if (!Wrapper::IsThis(value))
                return Value::ThrowTypeError(value.ctx, std::format("Expected {}", NameOf<T>()));

            T *out = Wrapper::Get(value);
            return out;
        }
    };

    template <typename T>
        requires (std::is_base_of_v<ManagedClass, T> || std::is_base_of_v<UnmanagedClass, T>)
    struct Conversion<RequireNonNull<T>> final {
        static constexpr bool Implemented = true;
        using Wrapper = Conversion<T *>::Wrapper;

        static Value Wrap(Context &ctx, RequireNonNull<T> cl) {
            return Wrapper::New(ctx, cl);
        }

        static JsResult<RequireNonNull<T>> Unwrap(Value const &value) {
            if (!Wrapper::IsThis(value))
                return Value::ThrowTypeError(value.ctx, std::format("Expected {}", NameOf<T>()));

            T *out = Wrapper::Get(value);
            return RequireNonNull<T>(out);
        }
    };

    template <typename T>
        requires Conversion<T>::Implemented
    struct Conversion<PassJsThis<T>> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, PassJsThis<T> const &value) {
            return value.jsThis;
        }

        static JsResult<PassJsThis<T>> Unwrap(Value value) {
            JsResult<T> result = Conversion<T>::Unwrap(value);
            if (!result.IsOk())
                return result.GetErr();
            return PassJsThis<T> {value, result.GetOk()};
        }
    };
    

    template <typename T>
        requires (std::is_base_of_v<ManagedClass, T> && std::is_copy_constructible_v<T>)
    struct Conversion<T> final {
        static constexpr bool Implemented = true;
        using Wrapper = Conversion<T *>::Wrapper;

        static Value Wrap(Context &ctx, T const &cl) {
            return Conversion<RequireNonNull<T>>::Wrap(ctx, RequireNonNull<T>(new T(cl)));
        }

        static JsResult<T> Unwrap(Value const &value) {
            auto res = Conversion<RequireNonNull<T>>::Unwrap(value);
            if (!res.IsOk())
                return res.GetErr();
            return *res.GetOk();
        }
    };

    template <typename T>
        requires Conversion<T>::Implemented
    struct Conversion<std::vector<T>> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, std::vector<T> const &vec) {
            Value arr = Value::Array(ctx);
            for (size_t i = 0; i < vec.size(); i++)
                arr[i] = Value::From(ctx, vec[i]);
            return arr;
        }

        static JsResult<std::vector<T>> Unwrap(Value &value) {
            auto lenRes = (*value["length"]).As<size_t>();
            if (!lenRes.IsOk())
                return lenRes.GetErr();

            size_t len = lenRes.GetOk();

            std::vector<T> out {};

            for (size_t i = 0; i < len; i++) {
                auto res = (*value[i]).As<T>();
                if (!res.IsOk())
                    return res.GetErr();
                out.push_back(res.GetOk());
            }

            return out;
        }
    };

    template <typename T, size_t TLen>
        requires Conversion<T>::Implemented
    struct Conversion<std::array<T, TLen>> final {
        static constexpr bool Implemented = true;

        static Value Wrap(Context &ctx, std::array<T, TLen> const &vec) {
            Value arr = Value::Array(ctx);
            for (size_t i = 0; i < vec.size(); i++)
                arr[i] = Value::From(ctx, vec[i]);
            return arr;
        }

        static JsResult<std::array<T, TLen>> Unwrap(Value &value) {
            auto lenRes = (*value["length"]).As<size_t>();
            if (!lenRes.IsOk())
                return lenRes.GetErr();

            size_t len = lenRes.GetOk();

            std::array<T, TLen> out {};

            for (size_t i = 0; i < len; i++) {
                auto res = (*value[i]).As<T>();
                if (!res.IsOk())
                    return res.GetErr();
                out[i] = res.GetOk();
            }

            for (size_t i = len; i < TLen; i++) {
                auto res = Value::Undefined(value.ctx).As<T>();
                if (!res.IsOk())
                    return res.GetErr();
                out[i] = res.GetOk();
            }
            
            return out;
        }
    };
}
