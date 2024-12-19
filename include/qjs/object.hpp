#pragma once

#include "qjs/value_fwd.hpp"
namespace Qjs {
    struct Object final {
        Value value;

        operator Value & () {
            return value;
        }

        Value &operator * () {
            return value;
        }

        Value *operator -> () {
            return &value;
        }
    };
}
