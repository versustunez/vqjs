#include "TypeScript.hpp"
#include "vqjs.h"

#include <File.h>
#include <algorithm>
#include <filesystem>
#include <format>
#include <quickjs/quickjs-libc.h>
#include <quickjs/quickjs.h>
#include <sstream>
#include <vector>

namespace VQJS {
#define LOGF(Method)                                                           \
  [](const Value &_, const std::vector<Value> &args) {                         \
    auto &logger = _.GetRuntime()->GetLogger();                                \
    for (const auto &item : args) {                                            \
      logger.Method(item.AsString());                                          \
    }                                                                          \
    return _.Undefined();                                                      \
  }

static constexpr auto ReadFile = [](const Value &_,
                                    const std::vector<Value> &args) {
  if (args.empty())
    return _.ThrowException("No File name provided");
  if (!args[0].IsString())
    return _.ThrowException("Argument type mismatch.");
  auto data = File::Read(args[0].AsString());
  if (!data.has_value())
    return _.ThrowException("Unable to read file");
  return _.String(data.value());
};

static constexpr auto WriteFile = [](const Value &_,
                                     const std::vector<Value> &args) {
  if (args.empty() && args.size() != 2)
    return _.ThrowException("Argument count mismatch");
  if (!args[0].IsString() || !args[1].IsString())
    return _.ThrowException("Arguments types mismatch.");
  const auto data = File::Write(args[0].AsString(), args[1].AsString());
  return _.Boolean(data);
};

static constexpr auto FileExists = [](const Value &_,
                                      const std::vector<Value> &args) {
  if (args.empty())
    return _.ThrowException("No File name provided");
  if (!args[0].IsString())
    return _.ThrowException("Argument type mismatch.");
  return _.Boolean(File::Exists(args[0].AsString()));
};

static void PrepareStd(const Context &context, bool allowFS) {
  Value global = Value::GlobalCtx(context);
  {
    auto consoleV = global.Object();
    consoleV.AddFunction("log", LOGF(Info));
    consoleV.AddFunction("debug", LOGF(Debug));
    consoleV.AddFunction("error", LOGF(Error));
    consoleV.AddFunction("warn", LOGF(Warn));
    global.Set("console", consoleV);
  }

  if (allowFS) {
    auto fs = global.Object();
    fs.AddFunction("read", ReadFile, 1);
    fs.AddFunction("write", WriteFile, 2);
    fs.AddFunction("exists", FileExists, 1);

    global.Set("fs", fs);
  }
}

struct Loader {
  static JSModuleDef *
  LoadModule(JSContext *ctx, const char *module_name, void *opaque) {
    auto *runtime = static_cast<Runtime *>(opaque);
    const auto val = runtime->LoadFile(module_name, false);
    if (val.IsException()) {
      runtime->GetLogger().Error(val.Exception().AsString());
      return nullptr;
    }
    return static_cast<JSModuleDef *>(val.GetUnderlyingPtr());
  }
};

Runtime::ModuleLoader::Resolved
Runtime::ModuleLoader::ResolvePath(const std::string &file) const {
  if (file[0] != '@') {
    auto base = Paths.at("@");
    auto path = std::filesystem::relative(file, base);
    return {base, path.generic_string()};
  }
  size_t firstSlash = file.find('/', 0);
  const std::string baseDir = file.substr(0, firstSlash);
  auto data = Paths.find(baseDir);
  if (data == Paths.end())
    return {baseDir, file.substr(firstSlash + 1)};
  return {data->second, file.substr(firstSlash + 1)};
}

Runtime::ModuleLoader &Runtime::ModuleLoader::Add(const std::string &a,
                                                  const std::string &b) {
  Paths[a] = b;
  return *this;
}

// It would be nice if there would be a native TS implementation inside C++
Runtime::Runtime() { m_Logger = CreateRef<Logger>(); }

bool Runtime::Start() { return Reset(); }

bool Runtime::Loop() const { return js_std_loop(m_AppInstance.m_Context); }
Runtime::Execution Runtime::LoopOnce() const {
  const int x = js_std_loop_once(m_AppInstance.m_Context);
  if (x == -2 || x == -1) {
    return {.erroredOrDone = true, .nextExecution = 0};
  }
  return {.erroredOrDone = false, .nextExecution = x};
}

static std::string join(const std::vector<std::string> &data,
                        const std::string_view &sep = ",") {
  std::ostringstream oss;
  for (size_t i = 0; i < data.size(); ++i) {
    if (i)
      oss << sep;

    oss << data[i];
  }
  return oss.str();
}

static void HandleRejectedPromisese(JSContext *ctx, JSValueConst,
                                           JSValueConst reason,
                                           bool is_handled, void *opaque) {
  if (is_handled) { return; }
  const auto* instance = static_cast<Runtime*>(opaque);
  const char *str = JS_ToCString(ctx, reason);
  if (str == nullptr) {
    return;
  }
  instance->GetLogger().Error(str);
  JS_FreeCString(ctx, str);
}

bool Runtime::Reset() {
  m_AppInstance.Reset();
  JS_SetRuntimeOpaque(m_AppInstance.m_Context, this);
  JS_SetHostPromiseRejectionTracker(m_AppInstance.m_Context, HandleRejectedPromisese, this);
  PrepareStd(m_AppInstance.m_Context, false);
  JS_SetModuleLoaderFunc(m_AppInstance.m_Context, nullptr, &Loader::LoadModule,
                         this);
  if (m_ModuleLoader.Paths.contains("@"))
    m_AppInstance.SetBaseDirectory(m_ModuleLoader.Paths["@"]);
  for (auto &path : m_ModuleLoader.Paths) {
    if (!File::Exists(path.second + ".cache/")) {
      File::CreateDirectory(path.second + ".cache/");
    }
  }
  return true;
}

Instance &Runtime::GetInstance() { return m_AppInstance; }

void Runtime::SetIncludeDirectory(const std::string &includeDir) {
  m_AppInstance.SetBaseDirectory(includeDir);
}

Value Runtime::LoadFile(const std::string &file, bool eval) const {
  return m_AppInstance.LoadFile(file, ModuleType::Module, eval);
}

static std::string
getCacheFileName(const Runtime::ModuleLoader::Resolved &resolved) {
  std::string cacheFilename = resolved.Extra;
  std::ranges::replace(cacheFilename, '/', '_');
  cacheFilename += ".js";
  return cacheFilename;
}

Runtime::CacheAndReal
Runtime::GetCacheAndRealPath(const std::string &file) const {
  auto resolvePath = m_ModuleLoader.ResolvePath(file);
  if (resolvePath.Base.empty()) {
    resolvePath.Base = m_AppInstance.m_BaseDirectory;
  }
  std::string cacheFile =
      resolvePath.Base + ".cache/" + getCacheFileName(resolvePath);
  std::string fullPath = resolvePath.Base + resolvePath.Extra;

  return {fullPath, cacheFile};
}

static void LogThunk(void *userdata, const char *msg) {
  if (userdata == nullptr) {
    return;
  }
  const auto *runtime = static_cast<Logger *>(userdata);
  runtime->Error(msg);
}

std::string Runtime::TranspileFile(const std::string &file) const {
  // we are happy now lets evolve
  auto path = GetCacheAndRealPath(file);
  if (File::Exists(path.Cache) &&
      File::LastChanged(path.Cache) > File::LastChanged(path.Real)) {
    return path.Cache;
  }

  // We can now use the oxc ts transpiler
  const auto logger =
      VQJSLog{.fn = &LogThunk, .data = static_cast<void *>(m_Logger.get())};
  if (!TypeScript::Transpile(path.Real, path.Cache, logger)) {
    m_Logger->Error(std::format("Failed to transpile \"{}\" file", path.Real));
  }

  return path.Cache;
}

std::optional<TS::ReflectionData>
Runtime::Metadata(const std::string &file) const {
  const auto logger =
      VQJSLog{.fn = &LogThunk, .data = static_cast<void *>(m_Logger.get())};
  return TypeScript::Metadata(file, logger);
}

void Runtime::WriteTSConfig() const {
  std::vector<std::string> paths{};
  std::vector<std::string> includes{};
  for (const auto &[fst, snd] : m_ModuleLoader.Paths) {
    std::string path =
        std::filesystem::relative(snd, m_AppInstance.m_BaseDirectory)
            .generic_string();
    paths.push_back(std::format(R"("{}/*": ["{}/*"])", fst, path));
    includes.push_back(std::format(R"("{}/**/*")", path));
  }
  // yes that's ugly, but we go for it :)
  const std::string fileContent = std::format(R"({{
  "compilerOptions": {{
    "target": "ES2023",
    "module": "ESNext",
    "experimentalDecorators": true,
    "useDefineForClassFields": false,
    "noImplicitAny": true,
    "strict": true,
    "allowJs": false,
    "alwaysStrict": true,
    "paths": {{{}}}
  }},
  "includes": [{}]
}})",
                                              join(paths), join(includes));
  File::Write(m_AppInstance.m_BaseDirectory + "tsconfig.json", fileContent);
}

void Runtime::SetLogger(const Ref<Logger> &logger) { m_Logger = logger; }
Logger &Runtime::GetLogger() const { return *m_Logger; }
Runtime::ModuleLoader &Runtime::GetLoader() { return m_ModuleLoader; }
} // namespace VQJS
