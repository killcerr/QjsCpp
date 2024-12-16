#pragma once

#include "quickjs.h"

namespace Qjs {
    struct Runtime final {
        JSRuntime *rt;

        Runtime() {
            rt = JS_NewRuntime();
            JS_SetRuntimeOpaque(rt, this);
        }

        Runtime(Runtime const &copy) = delete;

        operator JSRuntime *() {
            return rt;
        }

        static Runtime *From(JSRuntime *rt) {
            return static_cast<Runtime *>(JS_GetRuntimeOpaque(rt));
        }

        private:
        template <auto TNormalize>
        static char *Normalize(JSContext *ctx, char const *requestingSourceCstr, char const *requestedSourceCstr, void *opaque);

        template <auto TLoad>
        static JSModuleDef *Load(JSContext *ctx, const char *requestedSourceCstr, void *opaque);

        public:
        template <auto TNromalize, auto TLoad>
        void SetModuleLoaderFunc() {
            JS_SetModuleLoaderFunc(rt, Normalize<TNromalize>, Load<TLoad>, nullptr);
        }
    };
}
