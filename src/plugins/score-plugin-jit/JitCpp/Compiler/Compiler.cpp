#include <JitCpp/Compiler/Compiler.hpp>

#include <ossia/detail/flat_map.hpp>
#if defined(_WIN64)
#include "SectionMemoryManager.cpp"
#endif

// Add JITLink-specific headers
#include <llvm/ExecutionEngine/JITLink/EHFrameSupport.h>
#include <llvm/ExecutionEngine/JITLink/JITLink.h>
#include <llvm/ExecutionEngine/Orc/EHFrameRegistrationPlugin.h>
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>

namespace Jit
{
// TODO investigate https://stackoverflow.com/questions/1839965/dynamically-creating-functions-in-c
struct GlobalAtExit
{
  int nextCompilerID{};
  int currentCompiler{};
  ossia::flat_map<int, std::vector<void (*)()>> functions;
} globalAtExit;

static void jitAtExit(void (*f)())
{
  globalAtExit.functions[globalAtExit.currentCompiler].push_back(f);
}

void setTargetOptions(llvm::TargetOptions& opts)
{
  opts.EmulatedTLS = true;

#if LLVM_VERSION_MAJOR < 17
  opts.ExplicitEmulatedTLS = false;
#endif
  opts.UnsafeFPMath = true;
  opts.NoInfsFPMath = true;
  opts.NoNaNsFPMath = true;
  opts.NoTrappingFPMath = true;
  opts.NoSignedZerosFPMath = true;
  opts.HonorSignDependentRoundingFPMathOption = false;
  opts.EnableIPRA = false;
  opts.setFPDenormalMode(llvm::DenormalMode::getPositiveZero());
  opts.setFP32DenormalMode(llvm::DenormalMode::getPositiveZero());
  opts.EnableFastISel = false;
  opts.EnableGlobalISel = false;
}
}

namespace Jit
{

// Custom ObjectLinkingLayer plugin for handling atexit and other runtime functions
class RuntimeInterposePlugin : public llvm::orc::ObjectLinkingLayer::Plugin
{
public:
  RuntimeInterposePlugin(llvm::orc::MangleAndInterner& mangler)
      : m_mangler(mangler)
  {
  }

  void modifyPassConfig(
      llvm::orc::MaterializationResponsibility& MR, llvm::jitlink::LinkGraph& G,
      llvm::jitlink::PassConfiguration& Config) override
  {
    // Add a pass to handle runtime interposes
    Config.PreFixupPasses.push_back([this](llvm::jitlink::LinkGraph& G) -> llvm::Error {
      // Look for calls to atexit and redirect them
      for(auto* Sym : G.external_symbols())
      {
        if(Sym->getName() == m_mangler("atexit"))
        {
          // Create or find our jitAtExit symbol
          auto& atExitSym = G.addAbsoluteSymbol(
              "_jitAtExit", llvm::orc::ExecutorAddr::fromPtr(&jitAtExit),
              0, // size
              llvm::jitlink::Linkage::Strong, llvm::jitlink::Scope::Default,
              false // not callable
          );

          // Redirect references
          for(auto* B : G.blocks())
          {
            for(auto& E : B->edges())
            {
              if(&E.getTarget() == Sym)
              {
                E.setTarget(atExitSym);
              }
            }
          }
        }
      }
      return llvm::Error::success();
    });
  }

  llvm::Error notifyFailed(llvm::orc::MaterializationResponsibility& MR) override
  {
    qDebug() << "JITLink failed for " + MR.getTargetJITDylib().getName();
    return llvm::Error::success();
  }

  llvm::Error
  notifyRemovingResources(llvm::orc::JITDylib& JD, llvm::orc::ResourceKey K) override
  {
    return llvm::Error::success();
  }

  void notifyTransferringResources(
      llvm::orc::JITDylib& JD, llvm::orc::ResourceKey DstKey,
      llvm::orc::ResourceKey SrcKey) override
  {
  }

private:
  llvm::orc::MangleAndInterner& m_mangler;
};

class EagerLinkingPlugin : public llvm::orc::ObjectLinkingLayer::Plugin
{
public:
  explicit EagerLinkingPlugin(llvm::orc::ExecutionSession& ES)
      : ES{ES}
  {
  }

  void modifyPassConfig(
      llvm::orc::MaterializationResponsibility& MR, llvm::jitlink::LinkGraph& G,
      llvm::jitlink::PassConfiguration& Config) override
  {

    // Add a pass that collects all external symbols
    Config.PreFixupPasses.push_back(
        [this, &MR](llvm::jitlink::LinkGraph& G) -> llvm::Error {
      // Collect external symbols that need resolution
      llvm::orc::SymbolLookupSet ExternalSymbols;

      for(auto* Sym : G.external_symbols())
      {
        if((*Sym->getName()).starts_with("__lljit"))
          continue;
        if((*Sym->getName()).starts_with("atexit"))
          continue;
        ExternalSymbols.add(ES.intern(*Sym->getName()));
      }

      if(!ExternalSymbols.empty())
      {
        // Force resolution of external symbols now
        auto SearchOrder = makeJITDylibSearchOrder(&MR.getTargetJITDylib());
        auto Result = ES.lookup(SearchOrder, ExternalSymbols);

        if(!Result)
        {
          return Result.takeError();
        }

        //// Apply the resolved addresses
        for(auto* Sym : G.external_symbols())
        {
          auto It = Result->find(ES.intern(*Sym->getName()));
          if(It != Result->end())
          {

            //Sym->setAddress(It->second.getAddress());
          }
        }
      }

      return llvm::Error::success();
    });
  }

  llvm::Error notifyFailed(llvm::orc::MaterializationResponsibility& MR) override
  {
    qDebug() << "JITLink failed for " + MR.getTargetJITDylib().getName();
    return llvm::Error::success();
  }

  llvm::Error
  notifyRemovingResources(llvm::orc::JITDylib& JD, llvm::orc::ResourceKey K) override
  {
    return llvm::Error::success();
  }

  void notifyTransferringResources(
      llvm::orc::JITDylib& JD, llvm::orc::ResourceKey DstKey,
      llvm::orc::ResourceKey SrcKey) override
  {
  }

  llvm::orc::ExecutionSession& ES;
};

static std::unique_ptr<llvm::orc::LLJIT> jitBuilder(JitCompiler& self)
{
  auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
  SCORE_ASSERT(JTMB);

  llvm::orc::LLJITBuilder builder;

#if LLVM_VERSION_MAJOR < 18
  JTMB->setCodeGenOptLevel(llvm::CodeGenOpt::Aggressive);
#else
  JTMB->setCodeGenOptLevel(llvm::CodeGenOptLevel::Aggressive);
#endif

  setTargetOptions(JTMB->getOptions());

  builder.setJITTargetMachineBuilder(std::move(*JTMB));
  builder.setNumCompileThreads(4);

  // Configure to use JITLink via ObjectLinkingLayer
  builder.setObjectLinkingLayerCreator(
      [&](llvm::orc::ExecutionSession& ES, const llvm::Triple& TT) {
    // Create ObjectLinkingLayer with JITLink
    auto ObjLinkingLayer
        = std::make_unique<llvm::orc::ObjectLinkingLayer>(ES, *self.m_memmgr);

    ObjLinkingLayer->addPlugin(std::make_shared<RuntimeInterposePlugin>(self.m_mangler));
    ObjLinkingLayer->addPlugin(std::make_shared<EagerLinkingPlugin>(ES));

    // crash in deregisterEHFrames
    //ObjLinkingLayer->addPlugin(
    //    std::make_shared<llvm::orc::EHFrameRegistrationPlugin>(
    //        ES, std::make_unique<llvm::jitlink::InProcessEHFrameRegistrar>()));

    return ObjLinkingLayer;
  });

  auto p = builder.create();
  SCORE_ASSERT(p);
  if(!p)
    qDebug() << toString(p.takeError()).c_str();
  SCORE_ASSERT(p.get());
  return std::move(p.get());
}

JitCompiler::JitCompiler()
    : m_memmgr{std::move(*llvm::jitlink::InProcessMemoryManager::Create())}
    , m_jit{jitBuilder(*this)}
    , m_mangler{m_jit->getExecutionSession(), m_jit->getDataLayout()}
{
  using namespace llvm;
  using namespace llvm::orc;
  // Load own executable as dynamic library.
  // Required for RTDyldMemoryManager::getSymbolAddressInProcess().
  sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  LLJIT& JIT = *m_jit;
  m_jit->getExecutionSession().setErrorReporter([&](llvm::Error Err) {
    llvm::handleAllErrors(std::move(Err), [&](const llvm::ErrorInfoBase& EI) {
      const auto& mess = EI.message();
      this->m_errors += mess.c_str();
    });
  });
  auto& JD = JIT.getMainJITDylib();
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
  (void)m_jit->deinitialize(m_jit->getMainJITDylib());
}

void JitCompiler::compile(
    const std::string& cppCode, const std::vector<std::string>& flags,
    CompilerOptions opts, llvm::orc::ThreadSafeContext& context)
{
  using namespace llvm;
  using namespace llvm::orc;
  m_errors.clear();

  auto module
      = m_driver.compileTranslationUnit(cppCode, flags, opts, *context.getContext());

  if(!module)
  {
    throw Exception{module.takeError()};
  }

  if(auto Err = m_jit->addIRModule(ThreadSafeModule(std::move(*module), context));
     bool(Err))
  {
    throw Err;
  }

  globalAtExit.currentCompiler = globalAtExit.nextCompilerID++;
  m_atExitId = globalAtExit.currentCompiler;

  (void)m_jit->initialize(m_jit->getMainJITDylib());
}

}
