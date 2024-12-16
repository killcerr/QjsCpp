#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "quickjs.h"
#include "runtime_fwd.hpp"

namespace Qjs {
    struct Module;

    struct Context final {
        Runtime &rt;
        JSContext *ctx;

        std::vector<Module> modules;
        std::unordered_map<size_t, Module *> modulesByPtr;
        std::unordered_map<std::string, Module *> modulesByName;

        Context(Runtime &rt);

        operator JSContext *() {
            return ctx;
        }

        void ExecutePendingJob() {
            JS_ExecutePendingJob(rt, &ctx);
        }

        static Context *From(JSContext *ctx) {
            return static_cast<Context *>(JS_GetContextOpaque(ctx));
        }

        struct Value Eval(std::string src, std::string file, int flags = 0);

        Module &AddModule(std::string &&name);
    };
}
