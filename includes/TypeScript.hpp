#pragma once

#include "TypeScriptStructs.hpp"

#include <optional>
#include <string>

namespace VQJS {
struct TypeScript {
  /*
   * Transpile returns true if the translation of the file was successful.
   * Otherwise it will Log the error.
   */
  static auto Transpile(const std::string& file, const std::string& output,
                        VQJSLog logger) -> bool;
  /*
   * Metadata will return a Metadata object in future, it stores a list of
   * classes for this file, the class will also store all its members, functions
   * and more.
   *
   * if the extraction fails, we return an error object
   *
   */
  static auto Metadata(const std::string& file, VQJSLog logger)
      -> std::optional<TS::ReflectionData>;
};
} // namespace VQJS
