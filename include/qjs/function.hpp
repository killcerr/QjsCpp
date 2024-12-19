#pragma once

#include "qjs/result_fwd.hpp"
#include "qjs/value_fwd.hpp"
#include <type_traits>
#include <utility>

namespace Qjs {
    template <typename TReturn, typename ...TArgs>
    struct Function {
        Value value;

        JsResult<TReturn> operator () (TArgs &&...args) {
            return value.Invoke<TReturn>(std::forward<TArgs>(args)...);
        }
    };

    template <typename TReturn, typename TThis, typename ...TArgs>
    struct ThisFunction {
        Value value;

        JsResult<TReturn> operator () (TThis &&_this, TArgs &&...args) {
            return value.InvokeThis<TReturn, TThis>(std::forward<TThis>(_this), std::forward<TArgs>(args)...);
        }
    };
}
