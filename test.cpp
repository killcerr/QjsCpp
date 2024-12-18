#include "include/qjs.hpp"
#include "qjs/class.hpp"
#include "qjs/classbuilder_fwd.hpp"
#include "qjs/context_fwd.hpp"
#include "qjs/conversion.hpp"
#include "qjs/conversion_fwd.hpp"
#include "qjs/result_fwd.hpp"
#include "qjs/runtime_fwd.hpp"
#include "qjs/value_fwd.hpp"
#include <functional>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#define JS_SOURCE(...) #__VA_ARGS__

struct Test : public Qjs::ManagedClass {
    int x;
    float const y;

    Test(int x, float y) : x(x), y(y) {}
    ~Test() {
        std::println(std::cerr, "destruction");
    }

    void wawa(Qjs::Value jsThis, Qjs::PassJsThis<std::string> s) {
        std::println(std::cerr, "{}: {}, {}", *s, x, y);
    }
};

struct Unmanaged : public Qjs::UnmanagedClass {
    int x, y;

    Unmanaged(int x, int y) : x(x), y(y) {}
};

char const Src[] = JS_SOURCE(
    // import {wawa} from "test";
    import {Test} from "#test";

    log("I like girls", 1, 2, 3, 4, 5, 6);

    const test = new Test(2, 2.5);
    log(test.x, test.y);
    test.x += 1;
    log(test.x, test.y);
    test.wawa("wawa");
    let unmanaged = testFun([test, test, test, test]);
    log(unmanaged.x, unmanaged.y);
);

char const TestModSrc[] = JS_SOURCE(
    export function wawa() {
        log("test");
    }
);

Qjs::Value Log(Qjs::Value thisVal, std::vector<Qjs::Value> &args) {
    for (size_t i = 0; i < args.size(); i++) {
        auto strRes = args[i].ToString();
        if (!strRes.IsOk())
            return strRes.GetErr();

        std::print(std::cerr, "{}\t", strRes.GetOk());
    }
    std::println(std::cerr, "");

    return Qjs::Value::Undefined(thisVal.ctx);
}

Unmanaged unmanaged {1, 2};

Unmanaged *TestFun(std::vector<Qjs::RequireNonNull<Test>> t) {
    std::println(std::cerr, "{}", t.size());
    return &unmanaged;
}

std::string Normalize(Qjs::Context &ctx, std::string requesting, std::string requested) {
    return requested;
}

std::optional<std::string> Load(Qjs::Context &ctx, std::string requested) {
    if (requested == "test")
        return TestModSrc;
    return std::nullopt;
}

void RunTest(Qjs::Runtime &rt) {
    Qjs::Context ctx {rt};
    auto global = Qjs::Value::Global(ctx);

    global["log"] = Qjs::Value::RawFunction<Log>(ctx, "log");

    auto logfn = (*global["log"]).As<std::function<Qjs::JsResult<void> (std::string, int)>>().GetOk();
    logfn("test!", 5);

    auto &testMod = ctx.AddModule("#test");

    Qjs::ClassBuilder<Test>(ctx, "Test")
        .Ctor<int, float>()
        .Field<&Test::x>("x")
        .Field<&Test::y>("y")
        .Method<&Test::wawa>("wawa")
        .Build(testMod);

    Qjs::ClassBuilder<Unmanaged>(ctx, "TestUnmanaged")
        .Field<&Unmanaged::x>("x")
        .Field<&Unmanaged::y>("y")
        .Build(testMod);

    auto &test2Mod = ctx.AddModule("#test2");

    global["testFun"] = Qjs::Value::Function<TestFun>(ctx, "testFun");

    auto result = ctx.Eval(Src, "src.js");
    if (result.IsException())
        std::println(std::cerr, "{}", ctx.Eval(Src, "src.js").ExceptionMessage());
}

int main(int argc, char **argv) {
    Qjs::Runtime rt {true};
    rt.SetModuleLoaderFunc<Normalize, Load>();
    std::println(std::cerr, "test 2 begin");
    RunTest(rt);
    rt.Gc();
    std::println(std::cerr, "test 2 begin");
    RunTest(rt);

    return 0;
}
