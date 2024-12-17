#pragma once

#include "qjs/value_fwd.hpp"
#include <optional>

namespace Qjs {
    class Class {
        public:
        std::optional<Value> jsThis;
        bool managed = true;
    };
}
