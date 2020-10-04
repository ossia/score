#pragma once
#include <JitCpp/Compiler/Compiler.hpp>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/PrettyStackTrace.h>

namespace Jit
{

template <typename Fun_T>
struct Driver
{
  Driver(const std::string& fname)
      : X{0, nullptr}
      , ts_ctx{std::make_unique<llvm::LLVMContext>()}
      , jit{*llvm::EngineBuilder().selectTarget()}
      , factory_name{fname}
  {
  }

  std::function<Fun_T> operator()(
      const std::string& sourceCode,
      const std::vector<std::string>& flags,
      CompilerOptions opts)
  {
    auto t0 = std::chrono::high_resolution_clock::now();

    auto sourceFileName = saveSourceFile(sourceCode);
    if (!sourceFileName)
      return {};

    std::string cpp = *sourceFileName;
    auto filename = QFileInfo(QString::fromStdString(cpp)).fileName();

    auto module = jit.compile(cpp, flags, opts, ts_ctx);
    auto t1 = std::chrono::high_resolution_clock::now();

    auto jitedFn = jit.getFunction<Fun_T>(factory_name);
    if (!jitedFn)
      throw Exception{jitedFn.takeError()};

    llvm::outs().flush();
    std::cerr << "\n\nADDON BUILD DURATION: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                     .count()
              << " ms \n\n";

    return *jitedFn;
  }

  llvm::PrettyStackTraceProgram X;
  llvm::LLVMContext context;
  llvm::orc::ThreadSafeContext ts_ctx;
  JitCompiler jit;
  std::string factory_name;
};

}
