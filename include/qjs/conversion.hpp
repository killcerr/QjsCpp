#pragma once

#include "qjs/context.hpp"
#include "qjs/runtime.hpp"
#include "quickjs.h"
#include "value.hpp"

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

    template <typename T>
    struct JsClassWrapper {
        private:
        inline static JSClassID classId = 0;
        static inline std::vector<Value T::*> markOffsets;

        public:
        static JSClassID GetClassId(Runtime &rt) {
            if (classId != 0)
                return classId;

            JS_NewClassID(rt, &classId);
            return classId;
        }

        static void RegisterClass(Context &ctx, std::string &&name, JSClassCall *invoker = nullptr) {
            if (JS_IsRegisteredClass(ctx.rt, GetClassId(ctx.rt)))
                return;

            JSClassGCMark * marker = nullptr;
            // if (!markOffsets.empty()) {
            //     marker = [](JSRuntime *__rt, JSValue val, JS_MarkFunc *mark_func) {
            //         auto _rt = Runtime::From(__rt);
            //         if (!_rt)
            //             return;
            //         auto &rt = *_rt;

            //         auto ptr = static_cast<T>(JS_GetOpaque(val, GetClassId(rt)));
            //         if (!ptr)
            //             return;

            //         for (Value T::* member : markOffsets)
            //             JS_MarkValue(rt, (*ptr.*member).v, mark_func);
            //     };
            // }

            JSClassDef def{
                name.c_str(),
                [](JSRuntime *__rt, JSValue obj) noexcept {
                    auto _rt = Runtime::From(__rt);
                    if (!_rt)
                        return;
                    auto &rt = *_rt;
                    
                    auto ptr = static_cast<T*>(JS_GetOpaque(obj, GetClassId(rt)));
                    delete ptr;
                },
                marker,
                invoker,
                nullptr
            };

            JS_NewClass(ctx.rt, GetClassId(ctx.rt), &def);
        }

        static void SetProto(Context &ctx, Value proto) {
            JS_SetClassProto(ctx, GetClassId(ctx.rt), proto);
        }

        static Value New(Context &ctx, T *value) {
            auto obj = JS_NewObjectClass(ctx, GetClassId(ctx.rt));
            JS_SetOpaque(obj, value);
            return Value(ctx, obj);
        }

        static T *Get(Value value) {
            return static_cast<T *>(JS_GetOpaque(value, GetClassId(value.ctx.rt)));
        }
    };

    template <typename TReturn, typename ...TArgs>
    struct Conversion<std::function<TReturn(TArgs...)>> {
        static constexpr bool Implemented = true;

        using TFun = std::function<TReturn(TArgs...)>;
        using Class = JsClassWrapper<Conversion<TFun>>;
        std::function<TReturn(TArgs...)> fun;

        static JSValue Invoke(JSContext *__ctx, JSValue func_obj, JSValue this_val, int argc, JSValue *argv, int flags) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");

            auto &ctx = *_ctx;
            Value funcObj {ctx, func_obj};

            Conversion *conv = Class::Get(funcObj);

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
            Class::RegisterClass(ctx, "Function", Invoke);

            return Class::New(ctx, new Conversion {f});
        }

        static TFun Unwrap(Value value) {
            Class::RegisterClass(value.ctx, "Function", Invoke);

            if (JS_GetClassID(value) == Class::GetClassId(value.ctx.rt))
                return Class::Get(value)->fun;

            return [&](TArgs ...args) -> TReturn {
                return value.ToFunction<TReturn, TArgs...>()(args...).GetOk();
            };
        }
    };
}
