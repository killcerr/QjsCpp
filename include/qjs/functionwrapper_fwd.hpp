#pragma once

namespace Qjs {
    template <typename ...TArgs>
    struct UnpackWrapper;

    template <auto TFun>
    struct FunctionWrapper;

    template <auto TGetSet>
    struct GetSetWrapper;
}
