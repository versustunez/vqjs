#pragma once
#include <cstdint>

namespace VQJS::JS {
// Is Copied from QuickJS, we only removed the JS so we can use both.
typedef union ValueUnion {
  int32_t int32;
  double float64;
  void *ptr;
} ValueUnion;

typedef struct Value {
  ValueUnion u;
  int64_t tag;
} Value;
} // namespace VQJS::JS
