#include "vqjs.h"

#include <iostream>
auto main(const int argc, char *argv[]) -> int {
  if (argc == 1) {
    return 1;
  }
  VQJS::Runtime runtime;
  runtime.GetLoader().Add("@core", "./app-core/").Add("@", "./app/");
  VQJS::Instance &instance = runtime.GetInstance();
  runtime.Start();
  runtime.WriteTSConfig();
  instance.Global().AddFunction(
      "handleMetadata",
      [](const VQJS::Value &val, const std::vector<VQJS::Value> &) {
        std::cout << "Im Called from JS!"
                  << "\n";
        return val.New();
      });

  auto global = instance.Global();
  auto v3d = global.Object();
  global.Set("v3d", v3d);
  auto v3dAudio = global.Object();
  v3d.Set("audio", v3dAudio);
  v3dAudio.Set("left", global.TSharedArrayBuffer<double>(512));
  v3dAudio.Set("right", global.TSharedArrayBuffer<double>(512));

  VQJS::Value main = runtime.LoadFile("test.ts");
  if (main.IsException()) {
    std::cout << "main failed: " << main.Exception().AsString() << "\n";
  }
  return 0;
}
