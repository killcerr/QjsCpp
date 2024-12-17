#pragma once

#include "qjs/classwrapper_fwd.hpp"
#include "quickjs.h"
#include "value_fwd.hpp"
#include <format>

namespace Qjs {
    template <typename TClass, typename TValue, TValue (TClass::*TGetSet)>
    struct Value::GetSetWrapper<TGetSet> {
        static JSValue Get(JSContext *__ctx, JSValue this_val, int argc, JSValue *argv) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx)
                return JS_ThrowPlainError(__ctx, "Whar");
            auto &ctx = *_ctx;

            Value thisVal {ctx, this_val};

            TClass *t = ClassWrapper<TClass>::Get(thisVal);
            if (!t)
                return ThrowTypeError(ctx, std::format("Expected type {}.", NameOf<TClass>())).ToUnmanaged();

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
                return ThrowTypeError(ctx, std::format("Expected type {}.", NameOf<TClass>())).ToUnmanaged();

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

    template <auto TGet>
    void Value::AddGetter(Context &ctx, std::string &&name) {
        auto prop = JS_NewAtom(ctx, name.c_str());
        JS_DefinePropertyGetSet(ctx, value, prop,
            JS_NewCFunction(ctx, GetSetWrapper<TGet>::Get, name.c_str(), 0),
            JS_UNDEFINED,
        JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, prop);
    }

    template <auto TGet>
    void Value::AddGetterSetter(Context &ctx, std::string &&name) {
        auto prop = JS_NewAtom(ctx, name.c_str());
        JS_DefinePropertyGetSet(ctx, value, prop,
            JS_NewCFunction(ctx, GetSetWrapper<TGet>::Get, name.c_str(), 0),
            JS_NewCFunction(ctx, GetSetWrapper<TGet>::Set, name.c_str(), 1),
        JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, prop);
    }
}
