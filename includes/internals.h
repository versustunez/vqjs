#pragma once
#include <cstdint>

namespace VQJS::JS {
// Is Copied from QuickJS, we only removed the JS, so we can use both.
union ValueUnion {
  int32_t int32;
  double float64;
  void *ptr;
};

// Undefined Value by Default
struct Value {
  ValueUnion u{.int32 = 0};
  int64_t tag{3};
};
} // namespace VQJS::JS
