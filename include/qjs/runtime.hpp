#pragma once

#include "quickjs.h"

namespace Qjs {
    struct Runtime final {
        JSRuntime *rt;

        Runtime() {
            rt = JS_NewRuntime();
        }

        Runtime(Runtime const &copy) = delete;

        operator JSRuntime *() {
            return rt;
        }
    };
}
