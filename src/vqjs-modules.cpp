#include "vqjs-modules.h"

#include <iostream>

namespace VQJS {
// Default Logger VQJS Logger
void Logger::Info(const std::string &lines) const {
  std::cout << "[VQJS][Info] >> " << lines << "\n";
}

void Logger::Debug(const std::string &lines) const {
  std::cout << "[VQJS][Debug] >> " << lines << "\n";
}
void Logger::Error(const std::string &lines) const {
  std::cerr << "[VQJS][Error] >> " << lines << "\n";
}
void Logger::Warn(const std::string &lines) const {
  std::cout << "[VQJS][Warn] >> " << lines << "\n";
}
} // namespace VQJS
