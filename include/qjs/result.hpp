#pragma once

#include "result_fwd.hpp"
#include <optional>
#include "value_fwd.hpp"

namespace Qjs {
    template <typename T>
    Value JsResult<T>::GetErr() {
        return std::get<1>(Values);
    }

    template <>
    struct JsResult<void> {
        std::optional<Value> const Values;

        JsResult() : Values(std::nullopt) {}
        JsResult(Value &&err) : Values({err}) {}
        JsResult(Value const &err) : Values({err}) {}

        bool IsOk() {
            return !Values.has_value();
        }

        void GetOk() noexcept(false) {
            if (!IsOk())
                throw GetErr();
        }

        struct Value GetErr() {
            return *Values;
        }
    };
}
