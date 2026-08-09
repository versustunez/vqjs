#include "TypeScript.hpp"

#include <Vinary/Vinary.hpp>
#include <iostream>

namespace VQJS {

static constexpr uint16_t ArrayFlag = 1 << 0;
static constexpr uint16_t MapFlag = 1 << 1;

static constexpr uint32_t MAGIC = 0x53545156;
static constexpr uint32_t VERSION = 0x2;

namespace {

extern "C" {
struct BinaryBlob {
  uint8_t *ptr;
  uint64_t len;
};
bool vqjs_parse_file(const char *, const char *, VQJSLog logger);
BinaryBlob vqjs_reflect_metadata(const char *, VQJSLog logger);
void vqjs_reflect_free(uint8_t *ptr, uint64_t len);
}

struct RaiiBlob {
  explicit RaiiBlob(const BinaryBlob &value) : blob(value) {}
  ~RaiiBlob() { vqjs_reflect_free(blob.ptr, blob.len); }

  RaiiBlob(RaiiBlob &) = delete;

private:
  BinaryBlob blob;
};

} // namespace

bool TS::Type::isArray() const { return flags & ArrayFlag; }
bool TS::Type::isMap() const { return flags & MapFlag; }

auto TypeScript::Transpile(const std::string &file,
                           const std::string &output,
                           const VQJSLog logger) -> bool {
  // yes we instantly return this. we dont care at all
  return vqjs_parse_file(file.c_str(), output.c_str(), logger);
}

auto TypeScript::Metadata(const std::string &file, const VQJSLog logger)
    -> std::optional<TS::ReflectionData> {
  try {
    TS::ReflectionData data{};
    const auto returnValue = vqjs_reflect_metadata(file.c_str(), logger);
    RaiiBlob blob{returnValue};
    data.Load(returnValue.ptr, returnValue.len);
    return data;
  } catch (std::exception &exception) {
    // This is currently only to show why
    std::cerr << exception.what() << "\n";
    return {};
  }
}

static void WriteType(const TS::Type &type, Vinary::Writer &writer) {
  writer.writeString(type.name);
  writer.write(type.flags);
  writer.write<uint16_t>(type.value.has_value());
  if (type.value.has_value()) {
    if (const bool *val = get_if<bool>(&type.value.value())) {
      writer.write<uint16_t>(1);
      writer.write<uint16_t>(*val);
    }
    if (const double *val = get_if<double>(&type.value.value())) {
      writer.write<uint16_t>(2);
      writer.write<double>(*val);
    }
    if (const std::string *val = get_if<std::string>(&type.value.value())) {
      writer.write<uint16_t>(3);
      writer.writeString(*val);
    }
  }
  writer.write(type.arguments.size());
  for (auto &arg : type.arguments) {
    WriteType(arg, writer);
  }
}

static TS::Type ReadType(Vinary::Reader &reader) {
  TS::Type type;
  type.name = reader.readString();
  type.flags = reader.read<uint16_t>();
  if (reader.read<uint16_t>() == 1) {
    switch (reader.read<uint16_t>()) {
    case 1: {
      type.value.emplace<bool>(reader.read<uint16_t>() == 1);
      break;
    }
    case 2: {
      type.value.emplace<double>(reader.read<double>());
      break;
    }
    case 3: {
      type.value.emplace<std::string>(reader.readString());
      break;
    }
    default: break;
    }
  }
  const auto arguments = reader.read<uint64_t>();
  type.arguments.reserve(arguments);
  for (uint64_t i = 0; i < arguments; ++i) {
    type.arguments.push_back(ReadType(reader));
  }
  return type;
}

std::vector<std::byte> TS::ReflectionData::Save() const {
  Vinary::Writer writer;
  writer.write(MAGIC);
  writer.write(VERSION);
  writer.write(total);
  for (const auto &class_ : classes) {
    writer.writeString(class_.name);
    writer.writeString(class_.superClass);
    writer.write<uint64_t>(class_.member.size());
    for (auto &member : class_.member) {
      writer.writeString(member.name);
      WriteType(member.type, writer);
    }
  }
  return writer.buffer();
}

bool TS::ReflectionData::Load(const void *data, size_t size) {
  Vinary::Reader reader{data, size};
  try {
    auto magic = reader.read<uint32_t>();
    auto version = reader.read<uint32_t>();
    if (magic != MAGIC && version != VERSION) {
      return false;
    }
    // we need to load now the fields
    total = reader.read<uint64_t>();
    classes.reserve(total);
    for (size_t p = 0; p < total; ++p) {
      // this happens if we write a vector for free
      Class element = {.name = reader.readString(),
                       .superClass = reader.readString(),
                       .member = std::vector<Member>()};
      const auto memberCount = reader.read<uint64_t>();
      element.member.reserve(memberCount);
      for (uint64_t i = 0; i < memberCount; ++i) {
        element.member.push_back(
            {.name = reader.readString(), .type = ReadType(reader)});
      }
      classes.push_back(element);
    }
    return true;
  } catch (std::runtime_error &) { return false; }
}

} // namespace VQJS
