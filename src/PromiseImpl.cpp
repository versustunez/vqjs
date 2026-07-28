#include "impl.h"
#include "internals.h"
#include "quickjs/quickjs-libc.h"
#include "quickjs/quickjs.h"
#include "vqjs.h"

#include <iostream>
#include <utility>

#define FROM(obj) Utils::FromJSValue(obj)
#define TO(obj) Utils::ToJSValue(obj)

namespace VQJS {

Promise::Promise(Value value) : m_Value(std::move(value)) {}

Value Promise::Get() { return m_Value; }
typedef struct JSRefCountHeader {
  int ref_count;
} JSRefCountHeader;

Value Promise::Await() const {
  m_Value.Live();
  const auto result =
      FROM(js_std_await(m_Value.m_Context, TO(m_Value.m_UnderlyingValue)));
  auto res = Value{m_Value.m_Context, result};
  res.Live();
  return res;
}

Value Promise::Result() const {
  return Value{
      m_Value.m_Context,
      FROM(JS_PromiseResult(m_Value.m_Context, TO(m_Value.m_UnderlyingValue)))};
}

} // namespace VQJS

#undef FROM
#undef TO