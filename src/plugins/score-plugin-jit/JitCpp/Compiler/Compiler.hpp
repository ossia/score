#pragma once
#include <JitCpp/ClangDriver.hpp>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>

#include <QDebug>
namespace Jit
{
class JitCompiler
{
  using ModulePtr_t = std::unique_ptr<llvm::Module>;
public:
  JitCompiler(llvm::TargetMachine& targetMachine)
  {
    using namespace llvm;
    using namespace llvm::orc;
    // Load own executable as dynamic library.
    // Required for RTDyldMemoryManager::getSymbolAddressInProcess().
    sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    LLJIT& JIT = *m_jit;
    auto &JD = JIT.getMainJITDylib();
#if defined(SCORE_DEBUG)
    {
       auto s = absoluteSymbols({ { m_mangler("atexit"), JITEvaluatedSymbol(pointerToJITTargetAddress(&::atexit), JITSymbolFlags::Exported)}});
       (void)JD.define(std::move(s));
    }
#endif

    (void)m_overrides.enable(JD, m_mangler);

    {
      auto gen =
          DynamicLibrarySearchGenerator::GetForCurrentProcess(
            m_dl.getGlobalPrefix(),
            [&](const SymbolStringPtr &S) {
        return true;
      });
      JD.addGenerator(std::move(*gen));
    }
    {
      // auto gen =
      //     DynamicLibrarySearchGenerator::Load("/usr/lib/libc.so.6",
      //       DL.getGlobalPrefix(),
      //       [&](const SymbolStringPtr &S) {
      //   return true;
      // });
      // JD.addGenerator(std::move(*gen));
    }
  }

  ~JitCompiler()
  {
#if LLVM_VERSION_MAJOR >= 11
    (void)m_jit->deinitialize(m_jit->getMainJITDylib());
#else
    m_jit->runDestructors();
#endif
  }

  auto compile(
      const std::string& cppCode,
      const std::vector<std::string>& flags,
      CompilerOptions opts,
      llvm::orc::ThreadSafeContext& context)
  {
    using namespace llvm;
    using namespace llvm::orc;
    auto module = m_driver.compileTranslationUnit(cppCode, flags, opts, *context.getContext());
    if (!module)
      throw Exception{module.takeError()};

    if (auto Err = m_jit->addIRModule(ThreadSafeModule(std::move(*module), context)); bool(Err))
      throw Err;

#if LLVM_VERSION_MAJOR >= 11
    (void)m_jit->initialize(m_jit->getMainJITDylib());
#else
    m_jit->runConstructors();
#endif
    return std::move(*module);
  }

  template <class Signature_t>
  llvm::Expected<std::function<Signature_t>> getFunction(std::string name)
  {
    auto& JIT = *m_jit;
    // Look up the JIT'd code entry point.
    auto EntrySym = JIT.lookup(name);
    if (!EntrySym)
      return EntrySym.takeError();

    // Cast the entry point address to a function pointer.
    auto *Entry = (Signature_t*)EntrySym->getAddress();
    return std::function<Signature_t>(Entry);
  }

private:
  ClangCC1Driver m_driver;
  std::unique_ptr<llvm::orc::LLJIT> m_jit{std::move(llvm::orc::LLJITBuilder().create().get())};

  const llvm::DataLayout &m_dl{m_jit->getDataLayout()};
  llvm::orc::MangleAndInterner m_mangler{m_jit->getExecutionSession(), m_dl};
  llvm::orc::LocalCXXRuntimeOverrides m_overrides;
};
}
