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
  static const JSMallocFunctions jsMallocFunctions = {
      Calloc, Malloc, Free, Realloc, mi_malloc_usable_size};
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

Context::Context(Instance *instance)
    : Rt(CreateRuntime()),
      Ctx(CreateContext(Rt)),
      Count(new int(1)) {
  JS_SetContextOpaque(Ctx, instance);
}
Context::Context(const Context &o) : Rt(o.Rt), Ctx(o.Ctx), Count(o.Count) {
  ++(*Count);
}

Context &Context::operator=(const Context &other) noexcept {
  if (&other == this) {
    return *this;
  }
  // If we already have an instance...
  // we have to release us first...
  // Otherwise we would leak :)
  if (Count) {
    Release();
  }
  ++(*other.Count);
  Count = other.Count;
  Ctx = other.Ctx;
  Rt = other.Rt;
  return *this;
}

void Context::Release() const {
  --(*Count);
  if (*Count <= 0) {
    delete Count;
    // raise(SIGTRAP);
    JS_FreeContext(Ctx);
    JS_FreeRuntime(Rt);
  }
}
Context::~Context() { Release(); }
// To avoid Deallocating the last instance for a Free Empty Context Constructor
// :)
static int StaticCount = 1;
Context::Context() : Rt(nullptr), Ctx(nullptr), Count(&StaticCount) {
  ++StaticCount;
}
} // namespace VQJS