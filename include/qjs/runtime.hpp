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
    };
}
