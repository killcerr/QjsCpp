#pragma once

#include "qjs/value_fwd.hpp"
#include "quickjs.h"
#include <string>

namespace Qjs {
    inline Value Context::Eval(std::string src, std::string file) {
        return Value(*this, JS_Eval(ctx, src.c_str(), src.size(), file.c_str(), JS_EVAL_TYPE_MODULE));
    }
}
