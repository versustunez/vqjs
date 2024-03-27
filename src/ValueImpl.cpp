#include "impl.h"
#include "vqjs.h"

#include <cstdint>
#include <quickjs/quickjs.h>

namespace VQJS {

#define FROM(obj) Utils::FromJSValue(obj)
#define TO(obj) Utils::ToJSValue(obj)
#define VNEW(obj)                                                              \
  Value { m_Context, Utils::FromJSValue(obj) }

// SharedBuffers are always uint8_t ptr, so we can easily delete them :)
static void DeallocateSharedBuffer(JSRuntime *, void *, void *ptr) {
  delete[] static_cast<uint8_t *>(ptr);
}

Value Value::Global() const {
  return Value{m_Context, FROM(JS_GetGlobalObject(m_Context))};
}
Value Value::New() const { return VNEW(JS_UNDEFINED); }
Value Value::Undefined() const { return VNEW(JS_UNDEFINED); }
Value Value::Object() const { return VNEW(JS_NewObject(m_Context)); }
Value Value::NewArray() const { return VNEW(JS_NewArray(m_Context)); }

Value Value::SharedArrayBuffer(const size_t bytes) const {
  // we always creating shared Buffers here...
  auto *buffer = new uint8_t[bytes / sizeof(uint8_t)];
  const JSValue val = JS_NewArrayBuffer(m_Context, buffer, bytes,
                                        &DeallocateSharedBuffer, nullptr, true);
  return VNEW(val);
}

Value Value::SharedArrayBuffer(uint8_t *buf, const size_t bytes) const {
  const JSValue val = JS_NewArrayBuffer(m_Context, buf, bytes * sizeof(uint8_t),
                                        nullptr, nullptr, true);
  return VNEW(val);
}

std::string Value::AsString() const {
  const char *str = JS_ToCString(m_Context, TO(m_UnderlyingValue));
  if (str == nullptr) {
    return "null";
  }
  std::string result(str);
  JS_FreeCString(m_Context, str);
  return result;
}
double Value::AsDouble() const {
  double result;
  JS_ToFloat64(m_Context, &result, TO(m_UnderlyingValue));
  return result;
}

bool Value::AsBool() const {
  return JS_ToBool(m_Context, TO(m_UnderlyingValue));
}
int64_t Value::AsInt() const {
  int result;
  JS_ToInt32(m_Context, &result, TO(m_UnderlyingValue));
  return result;
}

std::vector<Value> Value::AsArray() const {
  std::vector<Value> data;
  JSValue array = TO(m_UnderlyingValue);
  const JSValue size =
      JS_GetPropertyStr(m_Context, TO(m_UnderlyingValue), "length");
  int len;
  JS_ToInt32(m_Context, &len, size);
  data.reserve(len);
  for (size_t i = 0; i < len; i++) {
    // No freeing needed... freeing is happening in Value Deconstructor!
    JSValue elem = JS_GetPropertyUint32(m_Context, array, i);
    data.emplace_back(m_Context, FROM(elem));
  }
  JS_FreeValue(m_Context, size);
  return data;
}

Value Value::Exception() const {
  return Value(m_Context, FROM(JS_GetException(m_Context)));
}
std::string Value::ExceptionStack() const {
  auto val = JS_GetPropertyStr(m_Context, TO(m_UnderlyingValue), "stack");
  const char *str = JS_ToCString(m_Context, val);
  JS_FreeValue(m_Context, val);
  if (!str) {
    return "<null>";
  }
  std::string result = str;
  JS_FreeCString(m_Context, str);
  return result;
}
Value Value::Get(const std::string &key) const { return (*this)[key]; }
Value Value::Call(const std::vector<Value> &args) const {
  const auto globalVal = Global();
  std::vector<JSValue> jsValues;
  jsValues.reserve(args.size());
  for (const auto &val : args) {
    jsValues.push_back(TO(val.m_UnderlyingValue));
  }
  const auto ret =
      JS_Call(m_Context, TO(m_UnderlyingValue), TO(globalVal.m_UnderlyingValue),
              static_cast<int>(args.size()), jsValues.data());
  return Value(m_Context, FROM(ret));
}

void Value::Set(const std::string &key, const Value &obj) const {
  JS_SetPropertyStr(m_Context, TO(m_UnderlyingValue), key.c_str(),
                    JS_DupValue(m_Context, TO(obj.m_UnderlyingValue)));
}

std::vector<std::string> Value::ObjectKeys() const {
  JSPropertyEnum *props;
  uint32_t len{0};

  const int ret = JS_GetOwnPropertyNames(
      m_Context, &props, &len, TO(m_UnderlyingValue), JS_GPN_STRING_MASK);
  if (ret < 0)
    return {};

  std::vector<std::string> keys;
  keys.reserve(len);
  for (int i = 0; i < len; i++) {
    Value val{m_Context, FROM(JS_AtomToString(m_Context, props[i].atom))};
    keys.push_back(val.AsString());
    JS_FreeAtom(m_Context, props[i].atom);
  }
  js_free(m_Context, props);
  return keys;
}

RawArray<void> Value::ToRawTypedArray() const {
  size_t bytesPerElement = 0;
  size_t byteLengths = 0;
  JSValue dataPtr =
      JS_GetTypedArrayBuffer(m_Context, TO(m_UnderlyingValue), nullptr,
                             &byteLengths, &bytesPerElement);

  return {dataPtr.u.ptr, byteLengths / bytesPerElement};
}

RawArray<void> Value::ToSharedArrayBuffer() const {
  size_t size = 0;
  uint8_t *ptr = JS_GetArrayBuffer(m_Context, &size, TO(m_UnderlyingValue));
  return {ptr, size};
}

// IsChecks
bool Value::IsObject() const { return JS_IsObject(TO(m_UnderlyingValue)); }
bool Value::IsString() const { return JS_IsString(TO(m_UnderlyingValue)); }
bool Value::IsNumber() const { return JS_IsNumber(TO(m_UnderlyingValue)); }
bool Value::IsBoolean() const { return JS_IsBool(TO(m_UnderlyingValue)); }

bool Value::IsException() const {
  return JS_IsException(TO(m_UnderlyingValue));
}
bool Value::IsArray() const {
  return JS_IsArray(m_Context, TO(m_UnderlyingValue));
}

Value Value::operator[](const std::string &name) const {
  const JSValue propValue =
      JS_GetPropertyStr(m_Context, TO(m_UnderlyingValue), name.c_str());
  return Value(m_Context, FROM(propValue));
}

Value Value::operator()(const std::vector<Value> &args) const {
  return Call(args);
}

static std::vector<Value> ConvertToValueCall(JSContext *ctx, JSValue *args,
                                             int argc) {
  std::vector<Value> valueArgs;
  valueArgs.reserve(argc);
  for (size_t i = 0; i < argc; i++) {
    valueArgs.push_back(Value::FromCtx(ctx, &args[i]));
  }
  return valueArgs;
}

struct ValueUtils {
  static JSValue cbHandler(JSContext *ctx, JSValue this_val, int argc,
                           JSValue *argv, int magic, JSValue *functionData) {
    constexpr JSClassID class_obj = 1;
    if (auto *funcPtr = static_cast<Value::Func *>(
            JS_GetOpaque2(ctx, functionData[0], class_obj))) {
      const Value val = (*funcPtr)(Value::FromCtx(ctx, &this_val),
                                   ConvertToValueCall(ctx, argv, argc));
      return JS_DupValue(ctx, TO(val.m_UnderlyingValue));
    }
    return JS_UNDEFINED;
  }
};

void Value::AddFunction(const std::string &name, const Func &func,
                        const size_t args) {
  // Create the JavaScript function with the C function pointer as the callback
  auto *instancePtr = static_cast<Instance *>(JS_GetContextOpaque(m_Context));
  JSValue fncData = JS_NewObject(m_Context);
  const std::string nameFncData = "__fncData" + name;
  JS_SetPropertyStr(m_Context, TO(m_UnderlyingValue), nameFncData.c_str(),
                    fncData);

  const Ref<Func> funcRef = CreateRef<Func>(func);
  JS_SetOpaque(fncData, funcRef.get());
  instancePtr->m_Functions.push_back(funcRef);

  const JSValue fnc = JS_NewCFunctionData(m_Context, &ValueUtils::cbHandler,
                                          args, 1, 1, &fncData);
  JS_SetPropertyStr(m_Context, TO(m_UnderlyingValue), name.c_str(), fnc);
}

Value Value::ThrowException(const std::string &message) const {
  return VNEW(JS_Throw(m_Context, JS_NewString(m_Context, message.c_str())));
}

Value Value::String(const std::string &data) const {
  return VNEW(JS_NewString(m_Context, data.c_str()));
}

Value Value::Boolean(bool value) const {
  return VNEW(JS_NewBool(m_Context, value));
}

Value Value::Number(double value) const {
  return VNEW(JS_NewFloat64(m_Context, value));
}

Value Value::From(const JSValue *value) const {
  return Value{m_Context, FROM(JS_DupValue(m_Context, *value))};
}

Runtime *Value::GetRuntime() const {
  return static_cast<Runtime *>(JS_GetRuntimeOpaque(JS_GetRuntime(m_Context)));
}

Value Value::FromCtx(JSContext *ctx, JSValue *val) {
  return Value{ctx, FROM(JS_DupValue(ctx, *val))};
}

Value Value::GlobalCtx(JSContext *ctx) {
  return Value{ctx, FROM(JS_GetGlobalObject(ctx))};
}

Value::Value() : m_Context(nullptr), m_UnderlyingValue(FROM(JS_UNDEFINED)) {}

Value::Value(JSContext *context)
    : m_Context(context),
      m_UnderlyingValue(FROM(JS_UNDEFINED)) {}

Value::Value(JSContext *context, const JS::Value val)
    : m_Context(context),
      m_UnderlyingValue(val) {}

Value::~Value() {
  if (m_Context) {
    JS_FreeValue(m_Context, TO(m_UnderlyingValue));
  }
}

Value::Value(const Value &val)
    : m_Context(val.m_Context),
      m_UnderlyingValue(
          FROM(JS_DupValue(m_Context, TO(val.m_UnderlyingValue)))) {}

Value::Value(Value &&val) noexcept
    : m_Context(val.m_Context),
      m_UnderlyingValue(val.m_UnderlyingValue) {
  val.m_UnderlyingValue = FROM(JS_UNDEFINED);
  val.m_Context = nullptr;
}

void *Value::GetUnderlyingPtr() const { return m_UnderlyingValue.u.ptr; }

#undef FROM
#undef TO
} // namespace VQJS
