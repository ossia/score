#include <JitCpp/Compiler/Compiler.hpp>
#include <map>
#if defined(_WIN64)
#include "SectionMemoryManager.cpp"
#endif
namespace Jit
{
// TODO investigate https://stackoverflow.com/questions/1839965/dynamically-creating-functions-in-c
struct GlobalAtExit {
  int nextCompilerID{};
  int currentCompiler{};
  std::map<int, std::vector<void(*)()>> functions;
} globalAtExit;
static void jitAtExit(void(*f)())
{
  globalAtExit.functions[globalAtExit.currentCompiler].push_back(f);
}

void setTargetOptions(llvm::TargetOptions& opts)
{
  opts.EmulatedTLS = false;
  opts.ExplicitEmulatedTLS = false;

  opts.UnsafeFPMath = true;
  opts.NoInfsFPMath = true;
  opts.NoNaNsFPMath = true;
  opts.NoTrappingFPMath = true;
  opts.NoSignedZerosFPMath = true;
  opts.HonorSignDependentRoundingFPMathOption = false;
  opts.EnableIPRA = true;
  opts.setFPDenormalMode(llvm::DenormalMode::getPositiveZero());
  opts.setFP32DenormalMode(llvm::DenormalMode::getPositiveZero());
}

static std::unique_ptr<llvm::orc::LLJIT> jitBuilder()
{
  auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
  SCORE_ASSERT(JTMB);

  llvm::orc::LLJITBuilder builder;
  JTMB->setCodeGenOptLevel(llvm::CodeGenOpt::Aggressive);
  setTargetOptions(JTMB->getOptions());

  builder.setJITTargetMachineBuilder(std::move(*JTMB));
  builder.setNumCompileThreads(4);

  auto p = builder.create();
  SCORE_ASSERT(p);
  if(!p)
    qDebug() << toString(p.takeError()).c_str();
  SCORE_ASSERT(p.get());
  return std::move(p.get());
}
JitCompiler::JitCompiler()
    : m_jit{jitBuilder()}
    , m_mangler{m_jit->getExecutionSession(), m_jit->getDataLayout()}
{
  using namespace llvm;
  using namespace llvm::orc;
  // Load own executable as dynamic library.
  // Required for RTDyldMemoryManager::getSymbolAddressInProcess().
  sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  LLJIT& JIT = *m_jit;
  m_jit->getExecutionSession().setErrorReporter([&] (llvm::Error Err) {
    llvm::handleAllErrors(std::move(Err), [&](const llvm::ErrorInfoBase& EI) {
      const auto& mess = EI.message();
      this->m_errors += mess.c_str();
    });
  });
  auto& JD = JIT.getMainJITDylib();

  {
    llvm::orc::SymbolMap RuntimeInterposes;

    RuntimeInterposes[m_mangler("atexit")] = {pointerToJITTargetAddress(&jitAtExit), JITSymbolFlags::Exported};

#if defined(_WIN64)
    RuntimeInterposes[m_mangler("fprintf")] = {pointerToJITTargetAddress(&::fprintf), JITSymbolFlags::Exported};
    RuntimeInterposes[m_mangler("vfprintf")] = {pointerToJITTargetAddress(&::vfprintf), JITSymbolFlags::Exported};
    RuntimeInterposes[m_mangler("__mingw_vfprintf")] = {pointerToJITTargetAddress(&::vfprintf), JITSymbolFlags::Exported};

    // RuntimeInterposes[m_mangler("_CxxThrowException")] = {pointerToJITTargetAddress(&SEHFrameHandler::RaiseSEHException), JITSymbolFlags::Exported};
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
        m_jit->getDataLayout().getGlobalPrefix(),
        [&](const SymbolStringPtr& S) { return true; });
    JD.addGenerator(std::move(*gen));
  }
}

JitCompiler::~JitCompiler()
{
  m_atExitId = globalAtExit.currentCompiler;
  // See https://lists.llvm.org/pipermail/llvm-dev/2017-December/119472.html for the order in which things must be done

  for(auto func : globalAtExit.functions[m_atExitId])
  {
    (*func)();
  }
  globalAtExit.functions.erase(m_atExitId);

  // TODO __dso_handle deinit ?
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
  m_errors.clear();

  auto module = m_driver.compileTranslationUnit(
      cppCode, flags, opts, *context.getContext());

  if (!module)
  {
    throw Exception{module.takeError()};
  }

  if (auto Err
      = m_jit->addIRModule(ThreadSafeModule(std::move(*module), context));
      bool(Err))
  {
    throw Err;
  }


  globalAtExit.currentCompiler = globalAtExit.nextCompilerID++;
  m_atExitId = globalAtExit.currentCompiler;

#if LLVM_VERSION_MAJOR >= 11
  (void)m_jit->initialize(m_jit->getMainJITDylib());
#else
  m_jit->runConstructors();
#endif
}

}
