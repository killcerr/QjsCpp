#pragma once

#include <functional>
#include <optional>
#include <string>

#include "quickjs.h"
#include "runtime.hpp"

namespace Qjs {
    using ModuleNormalizeFunc = std::function<std::string (std::string const &requestingSource, std::string const &requestedSource)>;
    using ModuleLoaderFunc = std::function<std::optional<std::string> (std::string const &path)>;

    struct Context final {
        Runtime &rt;
        JSContext *ctx;

        Context(Runtime &rt) : rt(rt) {
            ctx = JS_NewContext(rt);
            JS_SetContextOpaque(ctx, this);
        }

        operator JSContext *() {
            return ctx;
        }

        void ExecutePendingJob() {
            JS_ExecutePendingJob(rt, &ctx);
        }

        static Context *From(JSContext *ctx) {
            return static_cast<Context *>(JS_GetContextOpaque(ctx));
        }

        struct Value Eval(std::string src, std::string file);
    };
}
