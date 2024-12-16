#pragma once

#include "qjs/class.hpp"
#include "qjs/classwrapper_fwd.hpp"
#include "qjs/context_fwd.hpp"
#include "qjs/value_fwd.hpp"
#include "quickjs.h"
#include <string>
#include <type_traits>

namespace Qjs {
    template <typename T>
        requires std::is_base_of_v<Class, T>
    struct ClassBuilder final {
        private:
        Context &ctx;
        std::string const Name;
        Value prototype;
        Value ctor;

        public:
        ClassBuilder(Context &ctx, std::string &&name) : ctx(ctx), Name(name), prototype { Value::Object(ctx) }, ctor { Value::Undefined(ctx) } {
            ClassWrapper<T>::RegisterClass(ctx, std::string(Name));
            ClassWrapper<T>::SetProto(prototype);
        }

        private:
        template <auto TCtorFunc, typename ...TArgs>
        static JSValue CtorInvoke(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");
            auto &ctx = *_ctx;

            Value thisVal {ctx, this_val};
            
            JsResult<std::tuple<TArgs...>> optArgs = Value::UnpackArgs<TArgs...>(ctx, argc, argv);

            if (!optArgs.IsOk())
                return JS_DupValue(ctx, optArgs.GetErr());

            T *value = std::apply(TCtorFunc, optArgs.GetOk());

            Value proto = thisVal.Prototype();

            Value obj {ctx, JS_NewObjectProtoClass(ctx, proto, ClassWrapper<T>::GetClassId(ctx.rt))};

            if (obj.IsException()) {
                delete value;
                return JS_DupValue(ctx, obj);
            }

            JS_SetOpaque(obj, value);

            return JS_DupValue(ctx, obj);
        }

        template <typename ...TArgs>
        static T *DefaultConstructor(TArgs &&...args) {
            return new T(args...);
        }

        public:
        template <typename ...TArgs>
        ClassBuilder &Ctor() {
            ctor = Value(ctx, JS_NewCFunction2(ctx, CtorInvoke<DefaultConstructor<TArgs...>, TArgs...>, Name.c_str(), sizeof...(TArgs), JS_CFUNC_constructor, 0));
            JS_SetConstructor(ctx, ctor, prototype);
            return *this;
        }

        private:
        template <auto TField>
        struct FieldTraits;

        template <typename TClass, typename TValue, TValue (TClass::*TField)>
        struct FieldTraits<TField> {
            static constexpr bool IsConst = std::is_const_v<TValue>;
        };
        public:

        template <auto TField>
        ClassBuilder &Field(std::string &&name) {
            if constexpr (FieldTraits<TField>::IsConst)
                prototype.AddGetter<TField>(ctx, std::move(name));
            else
                prototype.AddGetterSetter<TField>(ctx, std::move(name));

            return *this;
        }

        template <auto TFun>
        ClassBuilder &Method(std::string &&name) {
            prototype[name] = Value::Function<TFun>(ctx, std::move(name));

            return *this;
        }

        void Build(Value object) {
            object[Name] = ctor;
        }
    };
}
