#pragma once

#include <variant>
namespace Qjs {
    struct Value;

    template <typename T>
    struct JsResult final {
        private:
        std::variant<T, Value> const Values;

        public:
        JsResult(T ok) : Values({ok}) {}
        JsResult(Value &&err) : Values({err}) {}
        JsResult(Value const &err) : Values({err}) {}

        bool IsOk() {
            return Values.index() == 0;
        }

        T GetOk() noexcept(false) {
            if (!IsOk())
                throw GetErr();
            return std::get<0>(Values);
        }

        T OkOr(T &&other) {
            if (IsOk())
                return GetOk();
            return other;
        }

        Value GetErr();
    };
}
