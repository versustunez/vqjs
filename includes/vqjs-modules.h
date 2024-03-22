#pragma once
#include <string>
namespace VQJS {

struct Logger {
  virtual ~Logger() = default;
  virtual void Info(const std::string &) const;
  virtual void Debug(const std::string &) const;
  virtual void Error(const std::string &) const;
  virtual void Warn(const std::string &) const;
};
} // namespace VQJS
