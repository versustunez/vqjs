#include "File.h"

#include <filesystem>
#include <fstream>

namespace VQJS {
bool File::Exists(const std::string &file) {
  return std::filesystem::exists(file);
}

std::optional<std::string> File::Read(const std::string &file) {
  std::ifstream input{file};
  if (!input.is_open())
    return {};
  std::string fileContents((std::istreambuf_iterator(input)),
                           std::istreambuf_iterator<char>());
  input.close();

  return fileContents;
}

bool File::Write(const std::string &file, const std::string &content) {
  std::ofstream outfile(file);
  if (!outfile.is_open())
    return false;
  outfile << content;
  return true;
}

std::string File::GetExtension(const std::string &file) {
  return std::filesystem::path(file).extension().generic_string();
}

size_t File::LastChanged(const std::string &file) {
  return std::filesystem::last_write_time(file).time_since_epoch().count();
}

std::string File::GetName(const std::string &file) {
  return std::filesystem::path(file).filename().generic_string();
}

bool File::CreateDirectory(const std::string &file) {
  return create_directories(std::filesystem::path(file));
}

} // namespace VQJS