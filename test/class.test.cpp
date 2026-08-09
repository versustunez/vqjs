#include "../includes/vqjs.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

TEST_CASE("IsOk") { REQUIRE(true); }

TEST_CASE("Can Create Class", "class") {
  VQJS::Runtime rt;
  rt.Start();
  auto classInst = rt.GetInstance().CreateClass("vqjs_class", false);
  auto proto = classInst->GetProto();
  proto.AddFunction(
      "Create",
      [](const VQJS::Value &val, const std::vector<VQJS::Value> &) {
        return val.Undefined();
      },
      0);
  classInst->GetProto().Set("meow", proto.Object());

  auto ins = classInst->New();
  REQUIRE(!ins.IsException());
  auto result = rt.Eval("const x = new vqjs_class();");
  REQUIRE(!result.IsException());
}

TEST_CASE("Can Create Class and Extend it in JS land", "class") {
  VQJS::Runtime rt;
  rt.Start();
  auto classInst = rt.GetInstance().CreateClass("vqjs_class", false);
  auto proto = classInst->GetProto();

  auto ins = classInst->New();
  REQUIRE(!ins.IsException());
  auto result =
      rt.Eval("class y extends vqjs_class { meow = 'hehe' }; globalThis.ins = "
              "new y();");
  REQUIRE(!result.IsException());
  REQUIRE(!result.Global()["ins"].IsException());
  REQUIRE(result.Global()["ins"]["meow"].IsString());
}

// Shows the error throw ;)
TEST_CASE("Cant instaniate class", "class") {
  VQJS::Runtime rt;
  rt.Start();
  auto classInst = rt.GetInstance().CreateClass("vqjs_class", true);
  auto proto = classInst->GetProto();

  auto ins = classInst->New();
  REQUIRE(!ins.IsException());
  auto result = rt.Eval("class y extends vqjs_class { constructor() { super() "
                        "}; }; globalThis.ins = "
                        "new y();");
  REQUIRE(!result.IsException());
  REQUIRE(result.Global()["ins"].IsUndefined());
}
