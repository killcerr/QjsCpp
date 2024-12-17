#pragma once

#include "qjs/class.hpp"
#include "qjs/context_fwd.hpp"
#include "qjs/value_fwd.hpp"
#include "quickjs.h"
#include <optional>
#include <type_traits>

namespace Qjs {
    template <typename T>
    struct ClassWrapper {
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

            JSClassGCMark *marker = nullptr;
            marker = [](JSRuntime *__rt, JSValue val, JS_MarkFunc *mark_func) {
                auto _rt = Runtime::From(__rt);
                if (!_rt)
                    return;
                auto &rt = *_rt;

                auto ptr = static_cast<T *>(JS_GetOpaque(val, GetClassId(rt)));
                if (!ptr)
                    return;

                for (Value T::*member : markOffsets)
                    JS_MarkValue(rt, (*ptr.*member).value, mark_func);
                
                if constexpr (std::is_base_of_v<Class, T>)
                    if (ptr->jsThis)
                        JS_MarkValue(rt, ptr->jsThis->value, mark_func);
            };

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

        static void SetProto(Value proto);

        static Value New(Context &ctx, T *value, std::optional<Value> proto) {
            Value val = Value::CreateFree(ctx, JS_NewObjectClass(ctx, GetClassId(ctx.rt)));
            JS_SetOpaque(val, value);
            return val;
        }

        static T *Get(Value value);
    };
}
