#include "include/qjs.hpp"
#include "qjs/class.hpp"
#include "qjs/classbuilder.hpp"
#include "qjs/value_fwd.hpp"
#include <iostream>
#include <ostream>
#include <string>

#define JS_SOURCE(...) #__VA_ARGS__

int add(int a, int b) {
    return a + b;
}

struct Test : public Qjs::Class {
    int x;
    float const y;

    Test(int x, float y) : x(x), y(y) {}

    void wawa(std::string s) {
        std::println(std::cout, "{}: {}, {}", s, x, y);
    }
};

char const Src[] = JS_SOURCE(
    const test = new Test(2, 2.5);
    print(test.x);
    test.x += 1;
    print(test.x);
    test.wawa("wawa");
    testFun(test);
);

void Print(std::string str) {
    std::println(std::cout, "{}", str);
}

void TestFun(Test t) {
    std::println(std::cout, "wawa");
}

int main(int argc, char **argv) {
    Qjs::Runtime rt;
    Qjs::Context ctx {rt};

    auto global = Qjs::Value::Global(ctx);

    global["print"] = Qjs::Value::Function<Print>(ctx, "print");

    Qjs::ClassBuilder<Test>(ctx, "Test")
        .Ctor<int, float>()
        .Field<&Test::x>("x")
        .Field<&Test::y>("y")
        .Method<&Test::wawa>("wawa")
        .Build(global);

    global["testFun"] = Qjs::Value::Function<TestFun>(ctx, "testFun");

    ctx.Eval(Src, "src.js");

    return 0;
}
