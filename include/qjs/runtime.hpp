#pragma once

#include "qjs/context_fwd.hpp"
#include "quickjs.h"
#include "runtime_fwd.hpp"
#include <algorithm>
#include <optional>
#include <string>
#include "module.hpp" // IWYU pragma: keep

namespace Qjs {
    template <auto TNormalize>
    char *Runtime::Normalize(JSContext *__ctx, char const *requestingSourceCstr, char const *requestedSourceCstr, void *opaque) {
        Context *_ctx = Context::From(__ctx);
        if (!_ctx) {
            JS_ThrowPlainError(__ctx, "Whar");
            return nullptr;
        }

        auto &ctx= *_ctx;

        std::string requestingSource = requestingSourceCstr;
        std::string requestedSource = requestedSourceCstr;

        std::string out = TNormalize(ctx, requestingSource, requestedSource);

        char *outCstr = (char *)js_malloc(ctx, out.size() + 1);
        std::fill(outCstr, outCstr + out.size() + 1, 0);
        out.copy(outCstr, out.size());

        return outCstr;
    }

    template <auto TLoad>
    JSModuleDef *Runtime::Load(JSContext *__ctx, const char *requestedSourceCstr, void *opaque) {
        Context *_ctx = Context::From(__ctx);
        if (!_ctx)
            return nullptr;

        auto &ctx= *_ctx;

        std::string requestedSource = requestedSourceCstr;

        std::optional<std::string> src = TLoad(ctx, requestedSource);

        if (!src)
            return nullptr;

        auto res = ctx.Eval(*src, requestedSource, JS_EVAL_FLAG_COMPILE_ONLY);

        JSModuleDef *mod = reinterpret_cast<JSModuleDef *>(JS_VALUE_GET_PTR(res.value));

        auto metaVal = JS_GetImportMeta(ctx, mod);

        Value meta (ctx, metaVal);

        JS_FreeValue(ctx, meta);

        meta["url"] = Value::From(ctx, requestedSource);
        meta["main"] = Value::From(ctx, false);
            
        return mod;
    }
}
