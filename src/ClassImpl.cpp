#include "impl.h"
#include "quickjs/quickjs.h"
#include "vqjs.h"

#include <cstdint>
#include <utility>

#define FROM(obj) Utils::FromJSValue(obj)
#define TO(obj) Utils::ToJSValue(obj)

namespace VQJS {
static JSValue NoConstructZone(JSContext *ctx,
                               JSValueConst new_target,
                               int argc,
                               JSValueConst *argv) {
  return JS_ThrowTypeError(
      ctx,
      "Illegal constructor: Class is not allowed to be constructed directly");
}

static JSValue XConstructor(JSContext *ctx,
                            JSValueConst new_target,
                            int argc,
                            JSValueConst *argv) {
  JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if (JS_IsUndefined(proto) || JS_IsException(proto)) {
    return JS_UNDEFINED;
  }
  auto classId = JS_GetClassID(proto);
  JSValue obj = JS_NewObjectProtoClass(ctx, proto, classId);
  return obj;
}

Class::Class(std::string name, bool noConstruct, const Context &context)
    : m_Name(std::move(name)),
      m_NoConstruct(noConstruct),
      m_Context(context) {
  std::uint32_t parent{0};
  m_ClassId = JS_NewClassID(context, &parent);
  // we need to create a new name atom
  JSClassDef def{
      .class_name = m_Name.c_str(), .finalizer = nullptr, .gc_mark = nullptr};
  auto c = JS_NewClass(context, m_ClassId, &def);
  m_Proto = Value::GlobalCtx(m_Context).Object();
}

Value Class::GetProto() { return m_Proto; }

void Class::Finalize() {
  if (m_IsFinalized) {
    return;
  }
  m_Proto.Live();
  JS_SetClassProto(m_Context, m_ClassId, TO(m_Proto.m_UnderlyingValue));
  auto ctorFnc = m_NoConstruct ? &NoConstructZone : &XConstructor;
  JSValue data[1] = {JS_NewInt32(m_Context, static_cast<int32_t>(m_ClassId))};
  auto ctor = JS_NewCFunction2(m_Context, ctorFnc, m_Name.c_str(), 0,
                               JS_CFUNC_constructor, 0);

  JS_SetConstructor(m_Context, ctor, TO(m_Proto.m_UnderlyingValue));

  // yes we are here
  JSValue global = JS_GetGlobalObject(m_Context);
  JS_SetPropertyStr(m_Context, global, m_Name.c_str(), ctor);
  JS_FreeValue(m_Context, global);

  m_IsFinalized = true;
}

Value Class::New() {
  if (!m_IsFinalized) {
    Finalize();
  }
  auto instance = JS_NewObjectClass(m_Context, m_ClassId);

  return Value{m_Context, FROM(instance)};
}

std::uint32_t Class::GetID() { return m_ClassId; }

void *Class::GetOpaque(Value instance) {
  return JS_GetOpaque(TO(instance.m_UnderlyingValue), m_ClassId);
}

void Class::SetOpaque(Value instance, void *ptr) {
  JS_SetOpaque(TO(instance.m_UnderlyingValue), ptr);
}
} // namespace VQJS

#undef FROM
#undef TO
