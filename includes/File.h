#pragma once
#include <string>
#include <string_view>
#include <optional>

namespace VQJS {
struct File {
  static bool Exists(const std::string &file);
  static std::optional<std::string> Read(const std::string &file);
  static bool Write(const std::string& file, const std::string &content);
  static std::string GetExtension(const std::string &file);
  static size_t LastChanged(const std::string &file);
  static std::string GetName(const std::string & file);
  static bool CreateDirectory(const std::string& file);
};
} // namespace VQJS
