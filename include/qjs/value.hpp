#pragma once

#include "qjs/functionwrapper_fwd.hpp"
#include "quickjs.h"
#include "value_fwd.hpp"

namespace Qjs {
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
