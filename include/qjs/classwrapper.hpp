#pragma once

#include "classwrapper_fwd.hpp"
#include "qjs/class.hpp"
#include "quickjs.h"
#include <type_traits>

namespace Qjs {
    template <typename T>
    void ClassWrapper<T>::SetProto(Value proto) {
        JS_SetClassProto(proto.ctx, GetClassId(proto.ctx.rt), proto.ToUnmanaged());
    }

    template <typename T>
    T *ClassWrapper<T>::Get(Value const &value) {
        return static_cast<T *>(JS_GetOpaque(value, GetClassId(value.ctx.rt)));
    }

    template <typename T>
    bool ClassWrapper<T>::IsThis(Value const &value) {
        return GetClassId(value.ctx.rt) == JS_GetClassID(value);
    }

    template <typename T>
        requires std::is_base_of_v<UnmanagedClass, T>
    struct ClassDeleteTraits<T> {
        static constexpr bool ShouldDelete = false;
    };
}
