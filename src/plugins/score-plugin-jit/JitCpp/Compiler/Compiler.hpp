#pragma once
#include <JitCpp/ClangDriver.hpp>

#include <QDebug>

#include <llvm/ExecutionEngine/JITLink/JITLink.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>

namespace Jit
{
class JitCompiler
{
  using ModulePtr_t = std::unique_ptr<llvm::Module>;

public:
  JitCompiler();
  ~JitCompiler();

  void compile(
      const std::string& cppCode, const std::vector<std::string>& flags,
      CompilerOptions opts, llvm::orc::ThreadSafeContext& context);

  template <class Signature_t>
  llvm::Expected<std::function<Signature_t>> getFunction(std::string name)
  {
    auto& JIT = *m_jit;
    // Look up the JIT'd code entry point.
    auto EntrySym = JIT.lookup(name);
    if(!EntrySym)
      return EntrySym.takeError();

      // Cast the entry point address to a function pointer.
#if LLVM_VERSION_MAJOR <= 14
    auto* Entry = (Signature_t*)EntrySym->getAddress();
#else
    auto* Entry = (Signature_t*)(*EntrySym).getValue();
#endif
    return std::function<Signature_t>(Entry);
  }

  const QString& errors() const noexcept
  {
    return m_errors;
  }
  std::unique_ptr<llvm::jitlink::InProcessMemoryManager> m_memmgr;
  ClangCC1Driver m_driver;
  std::unique_ptr<llvm::orc::LLJIT> m_jit;

  llvm::orc::MangleAndInterner m_mangler;

  QString m_errors;

  int m_atExitId{};
};
}
