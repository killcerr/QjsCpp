#include "include/qjs.hpp"
#include "qjs/class.hpp"
#include "qjs/classbuilder.hpp"
#include "qjs/value_fwd.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#define JS_SOURCE(...) #__VA_ARGS__

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
    log(test.x, test.y);
    test.x += 1;
    log(test.x, test.y);
    test.wawa("wawa");
    testFun([test, test, test, test]);
);

Qjs::Value Log(Qjs::Value thisVal, std::vector<Qjs::Value> &args) {
    for (size_t i = 0; i < args.size(); i++) {
        auto strRes = args[i].ToString();
        if (!strRes.IsOk())
            return strRes.GetErr();

        std::print(std::cout, "{}\t", strRes.GetOk());
    }
    std::println(std::cout);

    return Qjs::Value::Undefined(thisVal.ctx);
}

void TestFun(std::vector<Test *> t) {
    std::println(std::cout, "{}", t.size());
}

int main(int argc, char **argv) {
    Qjs::Runtime rt;
    Qjs::Context ctx {rt};

    auto global = Qjs::Value::Global(ctx);

    global["log"] = Qjs::Value::RawFunction<Log>(ctx, "log");

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
