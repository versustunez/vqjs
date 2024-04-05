#include "quickjs/quickjs.h"
#include "vqjs.h"

#include <iostream>
#include <mimalloc/include/mimalloc.h>

namespace VQJS {

static void *Alloc(JSMallocState *s, size_t size) {
  void *ptr;
  assert(size != 0);

  if (s->malloc_size + size > s->malloc_limit)
    return nullptr;

  ptr = mi_malloc(size);
  if (!ptr)
    return nullptr;
  s->malloc_count++;
  s->malloc_size += mi_malloc_usable_size(ptr);
  return ptr;
}

static void Free(JSMallocState *s, void *ptr) {
  if (!ptr)
    return;

  s->malloc_count--;
  s->malloc_size -= mi_malloc_usable_size(ptr);
  mi_free(ptr);
}

static void *Realloc(JSMallocState *s, void *ptr, size_t size) {
  if (!ptr) {
    if (size == 0)
      return nullptr;
    return Alloc(s, size);
  }
  size_t old_size = mi_malloc_usable_size(ptr);
  if (size == 0) {
    s->malloc_count--;
    s->malloc_size -= old_size;
    mi_free(ptr);
    return nullptr;
  }

  ptr = mi_realloc(ptr, size);
  if (!ptr)
    return nullptr;

  s->malloc_size += mi_malloc_usable_size(ptr) - old_size;
  return ptr;
}

static JSRuntime *CreateRuntime() {
  static JSMallocFunctions jsMallocFunctions{Alloc, Free, Realloc,
                                             mi_malloc_usable_size};
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