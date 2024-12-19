#pragma once

#include "qjs/value_fwd.hpp"
namespace Qjs {
    struct Object final {
        Value value;

        operator Value & () {
            return value;
        }

        operator Value const & () const {
            return value;
        }

        Value &operator * () {
            return value;
        }

        Value const &operator * () const {
            return value;
        }

        Value *operator -> () {
            return &value;
        }

        Value const *operator -> () const {
            return &value;
        }
    };
}
