#pragma once

#include "qjs/context_fwd.hpp"
#include "qjs/conversion_fwd.hpp"
#include "qjs/functionwrapper_fwd.hpp"
#include "qjs/value_fwd.hpp"
#include "qjs/classwrapper_fwd.hpp"
#include "qjs/class.hpp"
#include "quickjs.h"
#include <string>
#include <type_traits>
#include "module.hpp"

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
        template <auto TCtorFunc, bool TPtr, typename ...TArgs>
        static JSValue CtorInvoke(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");
            auto &ctx = *_ctx;

            Value thisVal {ctx, this_val};
            
            JsResult<std::tuple<std::decay_t<TArgs>...>> optArgs = UnpackWrapper<std::decay_t<TArgs>...>::UnpackArgs(ctx, thisVal, argc, argv);

            if (!optArgs.IsOk())
                return optArgs.GetErr().ToUnmanaged();

            if constexpr (TPtr) {
                T *value = std::apply(TCtorFunc, optArgs.GetOk());

                Value proto = thisVal.Prototype();

                Value obj = Value::CreateFree(ctx, JS_NewObjectProtoClass(ctx, proto, ClassWrapper<T>::GetClassId(ctx.rt)));

                if (obj.IsException()) {
                    delete value;
                    return obj.ToUnmanaged();
                }

                JS_SetOpaque(obj, value);

                return obj.ToUnmanaged();
            } else {
                return Conversion<T>::Wrap(ctx, std::apply(TCtorFunc, optArgs.GetOk())).ToUnmanaged();
            }
        }

        static JSValue NoCtorInvoke(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
            return JS_ThrowPlainError(ctx, "Class can't be constructed");
        }

        template <auto TCtor>
        struct CtorWrapper;

        template <typename ...TArgs, T (*TCtor)(TArgs...)>
        struct CtorWrapper<TCtor> {
            static constexpr auto Invoke = CtorInvoke<TCtor, false, std::decay_t<TArgs>...>;
            static constexpr auto ArgCount = sizeof...(TArgs);
        };

        template <typename ...TArgs, T *(*TCtor)(TArgs...)>
        struct CtorWrapper<TCtor> {
            static constexpr auto Invoke = CtorInvoke<TCtor, true, std::decay_t<TArgs>...>;
            static constexpr auto ArgCount = sizeof...(TArgs);
        };

        template <typename ...TArgs>
        static T *DefaultConstructor(TArgs &&...args) {
            return new T(args...);
        }

        public:
        template <auto TCtor>
        ClassBuilder &CtorFun() {
            ctor = Value::CreateFree(ctx, JS_NewCFunction2(ctx, CtorWrapper<TCtor>::Invoke, Name.c_str(), CtorWrapper<TCtor>::ArgCount, JS_CFUNC_constructor, 0));
            JS_SetConstructor(ctx, ctor, prototype);
            return *this;
        }

        template <typename ...TArgs>
        ClassBuilder &Ctor() {
            return CtorFun<DefaultConstructor<TArgs...>>();
        }

        ClassBuilder &NoCtor() {
            ctor = Value::CreateFree(ctx, JS_NewCFunction2(ctx, NoCtorInvoke, Name.c_str(), 0, JS_CFUNC_constructor, 0));
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
            object[Name] = Value(ctor);
        }

        void Build(Module &mod) {
            mod.AddExport(std::string(Name), ctor);
        }
    };
}
