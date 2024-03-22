#include "impl.h"
#include "vqjs.h"

#include <File.h>
#include <quickjs/quickjs-libc.h>
#include <quickjs/quickjs.h>
#include <string>
#include <utility>

namespace VQJS {

#define FROM(obj) Utils::FromJSValue(obj)
#define TO(obj) Utils::ToJSValue(obj)

static JSValue EvalBuffer(JSContext *ctx, const char *buf, size_t buf_len,
                          const std::string &filename, int eval_flags,
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

static JSValue EvalFile(JSContext *ctx, const std::string &filename, int module,
                        bool eval) {

  auto *runtime =
      static_cast<Runtime *>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
  std::string extension = File::GetExtension(filename);
  std::string realFile = filename;
  if (extension == ".ts" || extension.empty()) {
    realFile =
        runtime->TranspileFile(filename + (extension.empty() ? ".ts" : ""));
  }
  int eval_flags;

  const auto fileData = File::Read(realFile);
  if (!fileData) {
    return JS_ThrowReferenceError(ctx, "cant load file %s", realFile.c_str());
  }
  size_t bufferLen = fileData->size();
  const char *buf = fileData->c_str();
  if (module > 0 || module == -1)
    eval_flags = JS_EVAL_TYPE_MODULE;
  else
    eval_flags = JS_EVAL_TYPE_GLOBAL;
  auto ret = EvalBuffer(ctx, buf, bufferLen, realFile, eval_flags, !eval);

  if (JS_IsException(ret)) {
    ret = JS_GetException(ctx);
    const Value val{ctx, FROM(JS_DupValue(ctx, ret))};
    runtime->GetLogger().Error(val.AsString());
    runtime->GetLogger().Error(val.ExceptionStack());
  }
  return ret;
}

static JSContext *CreateContext(JSRuntime *rt) {
  JSContext *ctx = JS_NewContext(rt);
  if (!ctx)
    return nullptr;
  return ctx;
}

static int ModuleTypeToNumber(ModuleType type) {
  switch (type) {
  case ModuleType::Global: return 0;
  case ModuleType::Module: return 1;
  case ModuleType::Detect: return -1;
  }
  return -1;
}

Value Instance::LoadFile(const std::string &file, ModuleType type,
                         bool eval) const {
  std::string realFile = file[0] == '@' ? file : m_BaseDirectory + file;
  return Value(m_Context, FROM(EvalFile(m_Context, realFile,
                                        ModuleTypeToNumber(type), eval)));
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
void Instance::SetStackSize(int64_t size) {
  JS_SetMaxStackSize(m_Runtime, size);
}

Instance::Instance()
    : m_Runtime(JS_NewRuntime()),
      m_Context(CreateContext(m_Runtime)) {
  JS_SetContextOpaque(m_Context, this);
}

Instance::Instance(std::string name)
    : m_Name(std::move(name)),
      m_Runtime(JS_NewRuntime()),
      m_Context(CreateContext(m_Runtime)) {
  JS_SetContextOpaque(m_Context, this);
}
Instance::~Instance() {
  JS_FreeContext(m_Context);
  JS_FreeRuntime(m_Runtime);
}

void Instance::Reset() {
  JS_FreeContext(m_Context);
  JS_FreeRuntime(m_Runtime);
  m_Runtime = JS_NewRuntime();
  m_Context = CreateContext(m_Runtime);
  JS_SetContextOpaque(m_Context, this);
}

void Instance::SetBaseDirectory(const std::string &directory) {
  m_BaseDirectory = directory;
}

#undef FROM
#undef TO

} // namespace VQJS
