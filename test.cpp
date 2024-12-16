#include "include/qjs.hpp"
#include "qjs/value.hpp"
#include <functional>
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

    std::function<int(int, int)> test = [](int a, int b) -> int {
        return a + b;
    };

    auto addFunc = Qjs::Value::From(ctx, test);
    auto wrappedAddFunc = addFunc.As<std::function<int(int, int)>>().GetOk();

    std::println(std::cout, "{}", wrappedAddFunc(1, 2));

    return 0;
}
