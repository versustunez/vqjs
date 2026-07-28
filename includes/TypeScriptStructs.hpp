#pragma once

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
typedef void (*VQJSLogFn)(void *user_data, const char *);
struct VQJSLog {
  VQJSLogFn fn;
  void *data;
};
}

namespace VQJS::TS {
struct Type {
  std::string name;
  uint16_t flags = 0;

  bool isArray() const;
  bool isMap() const;
};

struct Member {
  std::string name;
  Type type;
};

struct Class {
  std::string name;
  std::string superClass;
  std::vector<Member> member;
};

struct ReflectionData {
  uint64_t total = 0;
  std::vector<Class> classes;

  [[nodiscard]] std::vector<std::byte> Save() const;
  bool Load(const void *data, size_t size);
};

} // namespace VQJS::TS