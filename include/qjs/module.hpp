#pragma once

#include "qjs/context_fwd.hpp"
#include "qjs/value_fwd.hpp"
#include "quickjs.h"
#include <cstddef>
#include <string>
#include <unordered_map>

namespace Qjs {
    struct Module final {
        std::string const Name;
        std::unordered_map<std::string, Value> exports {};
        JSModuleDef *mod;

        Module(Context &ctx, std::string &&name) : Name(name) {
            mod = JS_NewCModule(ctx, Name.c_str(), LoadRaw);
        }

        ~Module() {
            exports.clear();
        }

        static int LoadRaw(JSContext *__ctx, JSModuleDef *m) {
            auto _ctx = Context::From(__ctx);
            if (!_ctx) {
                JS_ThrowPlainError(__ctx, "Whar");
                return -1;
            }
            auto &ctx = *_ctx;

            auto &mod = ctx.modules[ctx.modulesByPtr[size_t(m)]];
            return mod.Load();
        }

        int Load() {
            for (auto &pair : exports) {
                int res = JS_SetModuleExport(pair.second.ctx, mod, pair.first.c_str(), pair.second.ToUnmanaged());
                if (res < 0)
                    return res;
            }
            return 0;
        }

        operator JSModuleDef *() {
            return mod;
        }

        void AddExport(std::string &&name, Value value) {
            exports.insert({name, value});
            JS_AddModuleExport(value.ctx, mod, name.c_str());
        }
    };
}
