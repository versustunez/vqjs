#pragma once
#include <internals.h>
#include <quickjs/quickjs.h>

namespace VQJS {
struct Utils {
  static JS::Value FromJSValue(const JSValue &value) {
    JS::Value val;
    val.tag = value.tag;
    val.u.ptr = value.u.ptr;
    return val;
  }

  static JSValue ToJSValue(const JS::Value &value) {
    JSValue val;
    val.tag = value.tag;
    val.u.ptr = value.u.ptr;
    return val;
  }
};
} // namespace VQJS
