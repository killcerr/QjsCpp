#pragma once

namespace Qjs {
    template <typename T>
    struct Conversion final {
        static constexpr bool Implemented = false;
    };

    template <typename T>
    struct RequireNonNull;

    template <typename T>
    struct PassJsThis;
}
