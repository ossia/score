#include <JitCpp/Compiler/Compiler.hpp>

#if defined(_WIN64)
#include "SectionMemoryManager.cpp"
#endif
namespace Jit
{

JitCompiler::JitCompiler(llvm::TargetMachine& targetMachine)
{
  using namespace llvm;
  using namespace llvm::orc;
  // Load own executable as dynamic library.
  // Required for RTDyldMemoryManager::getSymbolAddressInProcess().
  sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  LLJIT& JIT = *m_jit;
  auto& JD = JIT.getMainJITDylib();

  {
    llvm::orc::SymbolMap RuntimeInterposes;

#if defined(SCORE_DEBUG)
    RuntimeInterposes[m_mangler("atexit")] = {pointerToJITTargetAddress(&::atexit), JITSymbolFlags::Exported};
#endif

#if defined(_WIN64)
    RuntimeInterposes[m_mangler("_CxxThrowException")] = {pointerToJITTargetAddress(&SEHFrameHandler::RaiseSEHException), JITSymbolFlags::Exported};
#endif

    if(!RuntimeInterposes.empty())
    {
      auto s = absoluteSymbols(std::move(RuntimeInterposes));
      (void)JD.define(std::move(s));
    }
  }

  (void)m_overrides.enable(JD, m_mangler);

  {
    auto gen = DynamicLibrarySearchGenerator::GetForCurrentProcess(
        m_dl.getGlobalPrefix(),
        [&](const SymbolStringPtr& S) { return true; });
    JD.addGenerator(std::move(*gen));
  }
}

JitCompiler::~JitCompiler()
{
#if LLVM_VERSION_MAJOR >= 11
  (void)m_jit->deinitialize(m_jit->getMainJITDylib());
#else
  m_jit->runDestructors();
#endif
}

void
JitCompiler::compile(const std::string& cppCode, const std::vector<std::string>& flags, CompilerOptions opts, llvm::orc::ThreadSafeContext& context)
{
  using namespace llvm;
  using namespace llvm::orc;
  auto module = m_driver.compileTranslationUnit(
      cppCode, flags, opts, *context.getContext());
  if (!module)
    throw Exception{module.takeError()};

  if (auto Err
      = m_jit->addIRModule(ThreadSafeModule(std::move(*module), context));
      bool(Err))
    throw Err;

#if LLVM_VERSION_MAJOR >= 11
  (void)m_jit->initialize(m_jit->getMainJITDylib());
#else
  m_jit->runConstructors();
#endif
}

}
