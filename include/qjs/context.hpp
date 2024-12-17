#pragma once

#include "qjs/value_fwd.hpp"
#include "quickjs.h"
#include <string>
#include "module.hpp"

namespace Qjs {
    inline Context::Context(Runtime &rt) : rt(rt) {
        ctx = JS_NewContext(rt);
        JS_SetContextOpaque(ctx, this);
    }

    inline Context::~Context() {
        modules.clear();
        modulesByName.clear();
        modulesByPtr.clear();
        JS_FreeContext(ctx);
    }

    inline Value Context::Eval(std::string src, std::string file, int flags) {
        return Value::CreateFree(*this, JS_Eval(ctx, src.c_str(), src.size(), file.c_str(), flags | JS_EVAL_TYPE_MODULE));
    }

    inline Module &Context::AddModule(std::string &&name) {
        auto last = modules.size();
        
        modules.emplace_back(*this, std::move(name));

        Module &mod = modules[last];

        modulesByPtr.insert({size_t(mod.mod), &mod});
        modulesByName.insert({name, &mod});

        return mod;
    }
}
