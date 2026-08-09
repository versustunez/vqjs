#include "impl.h"
#include "vqjs.h"

#include <File.h>
#include <format>
#include <iostream>
#include <quickjs/quickjs-libc.h>
#include <quickjs/quickjs.h>
#include <string>
#include <utility>

namespace VQJS {

#define FROM(obj) Utils::FromJSValue(obj)
#define TO(obj) Utils::ToJSValue(obj)

static JSValue EvalBuffer(JSContext *ctx,
                          const char *buf,
                          size_t buf_len,
                          const std::string &filename,
                          int eval_flags,
                          bool nonEval) {

  if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
    JSValue val = JS_Eval(ctx, buf, buf_len, filename.c_str(),
                          eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
    if (!JS_IsException(val)) {
      js_module_set_import_meta(ctx, val, 1, !nonEval);
      return nonEval ? val : JS_EvalFunction(ctx, val);
    }
  } else {
    return JS_Eval(ctx, buf, buf_len, filename.c_str(), eval_flags);
  }
  return JS_UNDEFINED;
}

static JSValue Eval(JSContext *ctx,
                    const std::string &content,
                    const char *realFile,
                    int module,
                    bool eval,
                    Runtime *runtime,
                    Instance *instance) {
  int eval_flags;
  size_t bufferLen = content.size();
  const char *buf = content.c_str();
  if (module > 0 || module == -1)
    eval_flags = JS_EVAL_TYPE_MODULE;
  else
    eval_flags = JS_EVAL_TYPE_GLOBAL;
  auto ret = EvalBuffer(ctx, buf, bufferLen, realFile, eval_flags, !eval);

  if (JS_IsException(ret)) {
    ret = JS_GetException(ctx);
    const Value val{instance->GetContext(), FROM(JS_DupValue(ctx, ret))};
    runtime->GetLogger().Error(std::format(
        "Exception thrown:\n{}\n{}", val.AsString(), val.ExceptionStack()));
  }

  return ret;
}

static JSValue
EvalFile(JSContext *ctx, const std::string &filename, int module, bool eval) {
  auto *instance = static_cast<Instance *>(JS_GetContextOpaque(ctx));
  auto *runtime =
      static_cast<Runtime *>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
  std::string extension = File::GetExtension(filename);
  std::string realFile = filename;
  if (extension == ".ts" || extension.empty()) {
    realFile =
        runtime->TranspileFile(filename + (extension.empty() ? ".ts" : ""));
  }

  const auto fileData = File::Read(realFile);
  if (!fileData) {
    return JS_ThrowReferenceError(ctx, "cant load file %s", realFile.c_str());
  }

  return Eval(ctx, fileData.value(), realFile.c_str(), module, eval, runtime,
              instance);
}

static JSValue EvalString(JSContext *ctx, const std::string &content) {
  auto *instance = static_cast<Instance *>(JS_GetContextOpaque(ctx));
  auto *runtime =
      static_cast<Runtime *>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));

  return Eval(ctx, content, "<unnamed>", JS_EVAL_TYPE_GLOBAL, true, runtime,
              instance);
}

static int ModuleTypeToNumber(ModuleType type) {
  switch (type) {
  case ModuleType::Global: return 0;
  case ModuleType::Module: return 1;
  case ModuleType::Detect: return -1;
  }
  return -1;
}

Value Instance::LoadFile(const std::string &file, ModuleType type, bool eval) {
  const std::string realFile = file[0] == '@' ? file : m_BaseDirectory + file;
  auto returnValue = Value(
      m_Context,
      FROM(EvalFile(m_Context, realFile, ModuleTypeToNumber(type), eval)));
  return returnValue;
}

Value Instance::LoadFileAndStoreModule(const std::string &file) {
  const std::string realFile = file[0] == '@' ? file : m_BaseDirectory + file;
  auto returnValue = Value(
      m_Context, FROM(EvalFile(m_Context, realFile,
                               ModuleTypeToNumber(ModuleType::Module), false)));
  if (returnValue.IsUndefined()) {
    return returnValue;
  }
  auto result = Value{
      m_Context,
      FROM(JS_EvalFunction(m_Context, TO(returnValue.m_UnderlyingValue)))};

  if (result.IsException()) {
    result = result.Exception();
    auto *runtime =
        static_cast<Runtime *>(JS_GetRuntimeOpaque(JS_GetRuntime(m_Context)));
    runtime->GetLogger().Error(std::format("Exception thrown:\n{}\n{}",
                                           result.AsString(),
                                           result.ExceptionStack()));
    return result.Undefined();
  }
  returnValue.Live();
  m_Modules[realFile] = returnValue;
  return result;
}

Value Instance::Eval(const std::string &content) {
  return Value{m_Context, FROM(EvalString(m_Context, content))};
}

Value Instance::Global() const {
  return Value(m_Context, FROM(JS_GetGlobalObject(m_Context)));
}
Value Instance::String(const std::string &data) const {
  JSValue val = JS_NewString(m_Context, data.c_str());
  return Value(m_Context, FROM(val));
}
Value Instance::Double(double data) const {
  JSValue val = JS_NewFloat64(m_Context, data);
  return Value(m_Context, FROM(val));
}
Value Instance::Bool(bool data) const {
  JSValue val = JS_NewBool(m_Context, data);
  return Value(m_Context, FROM(val));
}
Value Instance::Int32(int32_t data) const {
  JSValue val = JS_NewInt32(m_Context, data);
  return Value(m_Context, FROM(val));
}
Value Instance::Int64(int64_t data) const {
  JSValue val = JS_NewInt64(m_Context, data);
  return Value(m_Context, FROM(val));
}
Value Instance::Undefined() const {
  return Value(m_Context, FROM(JS_UNDEFINED));
}
Value Instance::GetException() const {
  if (JS_HasException(m_Context)) {
    auto ret = JS_GetException(m_Context);
    return Value{m_Context, FROM(JS_DupValue(m_Context, ret))};
  }
  return Undefined();
}

Ref<Class> Instance::CreateClass(const std::string &name, bool noConstruct) {
  auto it = m_DefinedClass.find(name);
  if (it == m_DefinedClass.end()) {
    return m_DefinedClass
        .emplace(name, CreateRef<Class>(name, noConstruct, m_Context))
        .first->second;
  }
  return it->second;
}

Value Instance::GetModuleProperty(const std::string &module,
                                  const std::string &property) {
  auto mod = m_Modules.find(module);
  if (mod == m_Modules.end()) {
    return Undefined();
  }
  auto x = mod->second;
  auto ns = JS_GetModuleNamespace(
      m_Context,
      static_cast<JSModuleDef *>(JS_VALUE_GET_PTR(TO(x.m_UnderlyingValue))));
  auto a = Value{m_Context, FROM(ns)};
  return Value{m_Context,
               FROM(JS_GetPropertyStr(m_Context, ns, property.c_str()))};
}

void Instance::SetStackSize(int64_t size) const {
  JS_SetMaxStackSize(m_Context, size);
}

Instance::Instance(std::string name)
    : m_Name(std::move(name)),
      m_Context(this) {
  JS_SetContextOpaque(m_Context, this);
}

Instance::~Instance() { Reset(); };

void Instance::Reset() {
  m_Functions.clear();
  const Context ctx{this};
  m_Context = ctx;
}

void Instance::SetBaseDirectory(const std::string &directory) {
  m_BaseDirectory = directory;
}
std::string &Instance::GetBaseDirectory() { return m_BaseDirectory; }
Context &Instance::GetContext() { return m_Context; }

#undef FROM
#undef TO

} // namespace VQJS
