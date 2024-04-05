#pragma once
#include "internals.h"
#include "vqjs-modules.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// QuickJS classes
struct JSRuntime;
struct JSContext;
struct JSValue;

namespace VQJS {

template <typename T> using Ref = std::shared_ptr<T>;
template <typename T, typename... Args>
constexpr Ref<T> CreateRef(Args &&...args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

struct RuntimeInitFailedException final : std::exception {
  [[nodiscard]] const char *what() const noexcept override {
    return "Failed to Init VQJS Runtime";
  }
};

struct ValueUtils;
struct Runtime;
struct Instance;

struct Context {
  Context();
  explicit Context(Instance *);
  ~Context();
  Context(const Context &);
  Context(Context &&) = delete;
  operator JSContext *() const { return Ctx; }
  operator JSRuntime *() const { return Rt; }
  Context &operator=(const Context &other) noexcept;
  Context &operator=(Context &&other) noexcept = delete;
  static void PrintStats();

private:
  JSRuntime *Rt;
  JSContext *Ctx;
  int *Count;
  void Release() const;
};

template <typename T> struct RawArray {
  T *Data{nullptr};
  size_t Size{0};
};

struct Value {
  typedef std::function<Value(const Value &, const std::vector<Value> &args)>
      Func;
  struct FunctionData {
    Context Ctx;
    Func Function;
  };

  [[nodiscard]] Value Global() const;
  [[nodiscard]] Value New() const;
  [[nodiscard]] Value Undefined() const;
  [[nodiscard]] Value Object() const;
  [[nodiscard]] Value NewArray() const;
  [[nodiscard]] Value SharedArrayBuffer(size_t bytes) const;
  [[nodiscard]] Value SharedArrayBuffer(uint8_t *buf, size_t elements) const;
  template <typename T>
  [[nodiscard]] Value TSharedArrayBuffer(const size_t elements) const {
    return SharedArrayBuffer(elements * sizeof(T));
  }

  [[nodiscard]] std::string AsString() const;
  [[nodiscard]] double AsDouble() const;
  [[nodiscard]] bool AsBool() const;
  [[nodiscard]] int64_t AsInt() const;
  [[nodiscard]] std::vector<Value> AsArray() const;
  [[nodiscard]] Value Exception() const;
  [[nodiscard]] std::string ExceptionStack() const;
  [[nodiscard]] Value Get(const std::string &key) const;
  [[nodiscard]] Value Call(const std::vector<Value> &args) const;
  [[nodiscard]] Value CallBind(const Value &bind,
                               const std::vector<Value> &args) const;
  void Set(const std::string &key, const Value &obj) const;
  [[nodiscard]] std::vector<std::string> ObjectKeys() const;

  [[nodiscard]] RawArray<void> ToRawTypedArray() const;
  [[nodiscard]] RawArray<void> ToSharedArrayBuffer() const;

  [[nodiscard]] bool IsObject() const;
  [[nodiscard]] bool IsString() const;
  [[nodiscard]] bool IsNumber() const;
  [[nodiscard]] bool IsBoolean() const;
  [[nodiscard]] bool IsException() const;
  [[nodiscard]] bool IsArray() const;
  [[nodiscard]] bool IsFunction() const;

  Value operator[](const std::string &name) const;
  Value operator()(const std::vector<Value> &args) const;
  Value &operator=(const Value &other) noexcept;

  template <typename T> struct IsValue : std::is_convertible<T, Value> {};
  template <typename... Args,
            typename = std::enable_if_t<std::conjunction_v<IsValue<Args>...>>>
  Value operator()(Args &&...args) const {
    std::vector<Value> argVec{std::forward<Args>(args)...};
    return Call(argVec);
  }

  template <typename T> [[nodiscard]] RawArray<T> AsTypedArray() const {
    return static_cast<RawArray<T>>(ToRawTypedArray());
  }

  void AddFunction(const std::string &name, const Func &, size_t args = 0);

  // Will Dup the value so its leaking
  // Example: For Function calls that return values to JS :)
  void Live() const;

  [[nodiscard]] Value ThrowException(const std::string &message) const;
  [[nodiscard]] Value String(const std::string &data) const;
  [[nodiscard]] Value Boolean(bool) const;
  [[nodiscard]] Value Number(double) const;

  [[nodiscard]] Runtime *GetRuntime() const;

  Value From(const JSValue *) const;
  static Value FromCtx(const Context &, JSValue *);
  static Value GlobalCtx(const Context &);

  explicit Value(const Context &);
  explicit Value(const Context &, JS::Value);

  Value();
  ~Value();
  Value(const Value &);
  Value(Value &&) noexcept;

  // Danger, Danger!!!
  [[nodiscard]] void *GetUnderlyingPtr() const;

protected:
  explicit Value(const Context &, JS::Value, JS::Value);
  void Release();
  Context m_Context{};
  JS::Value m_UnderlyingValue{};
  JS::Value m_Parent{};
  friend ValueUtils;
  friend Runtime;
};

template <typename T> struct Array : RawArray<T> {
  explicit Array(const Value &val) : RawArray<T>(), Val(val) {
    const auto tmp = val.ToSharedArrayBuffer();
    this->Data = static_cast<T *>(tmp.Data);
    this->Size = tmp.Size / sizeof(T);
  }
  T &operator[](const size_t index) const {
    assert(index < this->Size && index >= 0);
    return this->Data[index];
  }
  Value Val{};
};

enum class ModuleType { Global = 0, Module = 1, Detect = -1 };
struct Instance {
  [[nodiscard]] Value Global() const;
  [[nodiscard]] Value String(const std::string &data) const;
  [[nodiscard]] Value Double(double data) const;
  [[nodiscard]] Value Bool(bool data) const;
  [[nodiscard]] Value Int32(int32_t data) const;
  [[nodiscard]] Value Int64(int64_t data) const;
  [[nodiscard]] Value Undefined() const;

  void SetBaseDirectory(const std::string &directory);

  // 0 == no limit
  void SetStackSize(int64_t size = 0);
  explicit Instance(std::string name);
  ~Instance();
  Instance(Instance &) = delete;

  Context &GetContext();
  std::string &GetName() { return m_Name; }

protected:
  [[nodiscard]] Value LoadFile(const std::string &file, ModuleType type,
                               bool eval = true) const;

  void Reset();
  std::string m_BaseDirectory{"./"};
  std::string m_Name{"Unknown"};
  Context m_Context;

  std::vector<Ref<Value::FunctionData>> m_Functions{};

  friend Value;
  friend Runtime;
};

struct Runtime {
  struct Config {
    std::string CoreDirectory = ".vqjs/";
    bool UseTypescript = true;
    std::vector<std::string> CompilerAddons;
  };

  struct ModuleLoader {
    struct Resolved {
      std::string Base{};
      std::string Extra{};
    };
    Resolved ResolvePath(const std::string &file) const;
    std::unordered_map<std::string, std::string> Paths;
    ModuleLoader &Add(const std::string &, const std::string &);
  };

  Runtime();
  Runtime(const Runtime &) = delete;
  Runtime(Runtime &&) = delete;
  bool Start();
  bool Reset();
  [[nodiscard]] Value LoadFile(const std::string &file, bool eval = true) const;
  [[nodiscard]] std::string TranspileFile(const std::string &file) const;
  void WriteTSConfig() const;

  Instance &GetInstance();
  Instance &GetCompilerInstance();

  void SetIncludeDirectory(const std::string &directory);
  void SetLogger(Ref<Logger> &logger);
  Logger &GetLogger();
  Config &GetConfig();
  ModuleLoader &GetLoader();

protected:
  Config m_Config{};
  Instance m_CompilationInstance{"Compiler"};
  Instance m_AppInstance{"App"};
  ModuleLoader m_ModuleLoader{};
  Ref<Logger> m_Logger{};
};

} // namespace VQJS
