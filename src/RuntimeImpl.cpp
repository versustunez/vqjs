#include "vqjs.h"

#include <File.h>
#include <filesystem>
#include <format>
#include <quickjs/quickjs.h>
#include <vector>

namespace VQJS {
#define LOGF(Method)                                                           \
  [](const Value &_, const std::vector<Value> &args) {                         \
    auto &logger = _.GetRuntime() -> GetLogger();                              \
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

static void PrepareStd(JSContext *ctx, bool allowFS) {
  Value global = Value::GlobalCtx(ctx);
  const JSValue global_obj = JS_GetGlobalObject(ctx);
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

  JS_FreeValue(ctx, global_obj);
}

struct Loader {
  static JSModuleDef *LoadModule(JSContext *ctx, const char *module_name,
                                 void *opaque) {
    auto *runtime = static_cast<Runtime *>(opaque);
    const auto val = runtime->LoadFile(module_name, false);
    if (val.IsException()) {
      runtime->GetLogger().Error(val.Exception().AsString());
      return nullptr;
    }
    return static_cast<JSModuleDef *>(val.GetUnderlyingPtr());
  }
};

std::string Runtime::ModuleLoader::ResolvePath(const std::string &file) const {
  if (file[0] != '@')
    return file;
  size_t firstSlash = file.find('/', 0);
  const std::string baseDir = file.substr(0, firstSlash);
  auto data = Paths.find(baseDir);
  if (data == Paths.end())
    return file;
  return data->second + file.substr(firstSlash);
}
Runtime::ModuleLoader &Runtime::ModuleLoader::Add(const std::string &a,
                                                  const std::string &b) {
  Paths[a] = b;
  return *this;
}

// It would be nice if there would be a native TS implementation inside C++
Runtime::Runtime() {
  m_Logger = CreateRef<Logger>();
  JS_SetRuntimeOpaque(m_CompilationInstance.m_Runtime, this);
  PrepareStd(m_CompilationInstance.m_Context, true);
}

bool Runtime::Start() {
  if (m_Config.UseTypescript) {
    m_CompilationInstance.SetStackSize(0);
    m_CompilationInstance.SetBaseDirectory(m_Config.CoreDirectory);
    if (m_CompilationInstance.LoadFile("typescript.js", ModuleType::Global)
            .IsException()) {
      return false;
    }
    if (m_CompilationInstance.LoadFile("compile.js", ModuleType::Global)
            .IsException()) {
      return false;
    }
  }
  return Reset();
}

bool Runtime::Reset() {
  m_AppInstance.Reset();
  JS_SetRuntimeOpaque(m_AppInstance.m_Runtime, this);
  PrepareStd(m_AppInstance.m_Context, false);
  JS_SetModuleLoaderFunc(m_AppInstance.m_Runtime, nullptr, &Loader::LoadModule,
                         this);
  if (m_ModuleLoader.Paths.contains("@"))
    m_AppInstance.SetBaseDirectory(m_ModuleLoader.Paths["@"]);
  if (!File::Exists(m_AppInstance.m_BaseDirectory + ".cache/")) {
    File::CreateDirectory(m_AppInstance.m_BaseDirectory + ".cache/");
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

static std::string getCacheFileName(std::filesystem::path &diffPath) {
  std::string cacheFilename;
  for (const auto &component : diffPath) {
    if (!cacheFilename.empty())
      cacheFilename += "_";
    cacheFilename += component.string();
  }
  cacheFilename += ".js";
  return cacheFilename;
}

std::string Runtime::TranspileFile(const std::string &file) const {
  // so first lets check if there is a cached version already
  std::string realFile = m_ModuleLoader.ResolvePath(file);
  auto path =
      std::filesystem::relative(realFile, m_AppInstance.m_BaseDirectory);
  std::string cacheFile =
      m_AppInstance.m_BaseDirectory + ".cache/" + getCacheFileName(path);

  if (File::Exists(cacheFile) &&
      File::LastChanged(cacheFile) > File::LastChanged(file)) {
    return cacheFile;
  }
  if (!m_Config.UseTypescript)
    return file;

  // okay now we are in a TS scope ;)
  const Value _ = m_CompilationInstance.Global();
  _["compile"](_.String(realFile), _.String(cacheFile));
  return cacheFile;
}

void Runtime::WriteTSConfig() const {
  std::stringstream includes;
  includes << R"_("includes": [)_";
  std::stringstream config;
  config << R"_({"paths": {)_";
  char added = ' ';

  // this is not an problem at all. do not commit the tsconfig because it writes
  // user specific files
  for (const auto &[fst, snd] : m_ModuleLoader.Paths) {
    if (fst == "@")
      continue;
    std::string path =
        std::filesystem::relative(snd, m_AppInstance.m_BaseDirectory).string();
    config << added << '"' << fst << "/*\":\"" << path << "**/*\"";
    includes << added << '"' << path << "**/*\"";
    added = ',';
  }
  config << "}," << includes.str() << "]}";
  File::Write(m_AppInstance.m_BaseDirectory + "tsconfig.json", config.str());
}

void Runtime::SetLogger(Ref<Logger> &logger) { m_Logger = logger; }
Logger &Runtime::GetLogger() { return *m_Logger; }
Runtime::Config &Runtime::GetConfig() { return m_Config; }
Runtime::ModuleLoader &Runtime::GetLoader() { return m_ModuleLoader; }
} // namespace VQJS
