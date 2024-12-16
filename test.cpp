#include "include/qjs.hpp"
#include "qjs/value.hpp"
#include <iostream>
#include <ostream>

#define JS_SOURCE(...) #__VA_ARGS__

char const Src[] = JS_SOURCE(
    
);

int add(int a, int b) {
    return a + b;
}

int main(int argc, char **argv) {
    Qjs::Runtime rt;
    Qjs::Context ctx {rt};

    auto addFunc = Qjs::Value::Function<add>(ctx, "add");
    auto wrappedAddFunc = addFunc.ToFunction<int, int, int>();

    auto res = wrappedAddFunc(1, 2);

    if (res.IsOk())
        std::println(std::cout, "{}", res.GetOk());
    else
        std::println(std::cout, "{}", res.GetErr().ExceptionMessage());

    return 0;
}
