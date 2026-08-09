#include "quickjs/quickjs-libc.h"
#include "quickjs/quickjs.h"
#include "vqjs.h"

#include <iostream>
#include <mimalloc/include/mimalloc.h>

namespace VQJS {

static void *Calloc(void *opaque, size_t count, size_t size) {
  return mi_calloc(count, size);
}

static void *Malloc(void *opaque, size_t size) { return mi_malloc(size); }

static void Free(void *opaque, void *ptr) {
  if (!ptr)
    return;
  mi_free(ptr);
}

static void *Realloc(void *opaque, void *ptr, size_t size) {
  return mi_realloc(ptr, size);
}

static JSRuntime *CreateRuntime() {
  static constexpr JSMallocFunctions jsMallocFunctions = {
      Calloc, Malloc, Free, Realloc, mi_usable_size};
  return JS_NewRuntime2(&jsMallocFunctions, nullptr);
}

static JSContext *CreateContext(JSRuntime *rt) {
  if (!rt)
    return nullptr;
  JSContext *ctx = JS_NewContext(rt);
  if (!ctx)
    return nullptr;
  return ctx;
}

Context::Context(Instance *instance): m_State(std::make_shared<State>()) {
  JS_SetContextOpaque(*this, instance);
}

Context::State::State() : Rt(CreateRuntime()), Ctx(CreateContext(Rt)){
  js_std_init_handlers(Rt);
}
Context::State::~State() {
  js_std_free_handlers(Rt);
  JS_FreeContext(Ctx);
  JS_FreeRuntime(Rt);
}

Context::Context() = default;
} // namespace VQJS