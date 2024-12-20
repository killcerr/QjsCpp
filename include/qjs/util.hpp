#pragma once

#include <string>
#include <typeinfo>

#ifndef _WIN32
#include <cxxabi.h>
#endif

namespace Qjs {
#ifdef _WIN32
    /// MSVC doesn't mangle `typeid().name()`.
    template<typename T>
    std::string NameOf() {
        return typeid(T).name();
    }
#else
    /// https://gcc.gnu.org/onlinedocs/libstdc++/manual/ext_demangling.html
    template<typename T>
    std::string NameOf() {
        int status;
        char *demangled = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
        std::string out = demangled;
        std::free((void *)demangled);
        return out;
    }
#endif

    template <typename T>
    auto Unit(auto &&v) {
        return v;
    }
    template <typename T>
    void Void() {}
}
