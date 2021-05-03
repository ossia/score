#include <faust/dsp/llvm-dsp.h>
#include <memory>
#include <QFile>
int main(int argc, char** argv)
{

  const char* triple =
#if defined(_MSC_VER)
      "x86_64-pc-windows-msvc"
#elif defined(__emscripten__)
      "wasm32-unknown-unknown-wasm"
#elif defined(__aarch64__)
      ""
#elif defined(__arm__)
      "arm-none-linux-gnueabihf"
#else
      ""
#endif
      ;

  QFile f{argv[1]};
  f.open(QIODevice::ReadOnly);

  auto str = f.readAll().toStdString();

  std::string err;
  err.resize(4097);

  llvm_dsp_factory* fac{};

  {
    int argc = 0;
    const char* argv[1] = { nullptr };
    fac = createDSPFactoryFromString("score", str.c_str(), argc, argv, triple, err, -1);
    assert(fac);

    auto obj = fac->createDSPInstance();
    assert(obj);

    obj->init(44100);
  }
}
