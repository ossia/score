// Standalone reproducer for the Windows JIT COFF failure
// ("Could not find symbol at given index, ... index: 9, section: 45").
//
// It replicates score's JIT path (src/plugins/score-plugin-jit/JitCpp/Compiler/
// Compiler.cpp jitBuilder): the same hand-built ObjectLinkingLayer with the
// RuntimeInterpose + EagerLinking plugins and COFF responsibility flags, the same
// JITTargetMachineBuilder options, then loads a precompiled addon bitcode and
// JIT-links it -- with an ObjectTransformLayer that dumps the exact object ORC
// feeds to JITLink, so we can inspect symbol 9 / section 45.
//
// Build against the same LLVM as score (ossia-sdk). Run: jit_repro addon.bc

#include <llvm/ExecutionEngine/JITLink/JITLink.h>
#include <llvm/ExecutionEngine/JITLink/EHFrameSupport.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>

#include <atomic>
#include <cstdio>
#include <map>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

// --- atexit interpose (verbatim from score) ---
namespace {
struct GlobalAtExit { std::map<int, std::vector<void (*)()>> functions; } globalAtExit;
void jitAtExit(void (*f)()) { globalAtExit.functions[0].push_back(f); }

void setTargetOptions(TargetOptions& opts)
{
  opts.EmulatedTLS = true;
#if LLVM_VERSION_MAJOR < 22
  opts.UnsafeFPMath = true;
  opts.ApproxFuncFPMath = true;
  opts.setFPDenormalMode(DenormalMode::getPositiveZero());
  opts.setFP32DenormalMode(DenormalMode::getPositiveZero());
#else
  opts.AllowFPOpFusion = FPOpFusion::Fast;
#endif
  opts.NoInfsFPMath = true;
  opts.NoNaNsFPMath = true;
  opts.NoTrappingFPMath = true;
  opts.NoSignedZerosFPMath = true;
  opts.HonorSignDependentRoundingFPMathOption = false;
  opts.EnableIPRA = true;
  opts.EnableFastISel = true;
  opts.EnableGlobalISel = false;
}

class RuntimeInterposePlugin : public ObjectLinkingLayer::Plugin
{
public:
  RuntimeInterposePlugin(MangleAndInterner& mangler) : m_mangler(mangler) {}
  void modifyPassConfig(
      MaterializationResponsibility&, jitlink::LinkGraph&,
      jitlink::PassConfiguration& Config) override
  {
    Config.PreFixupPasses.push_back([this](jitlink::LinkGraph& G) -> Error {
      for(auto* Sym : G.external_symbols())
        if(Sym->getName() == m_mangler("atexit"))
        {
          auto& atExitSym = G.addAbsoluteSymbol(
              "_jitAtExit", ExecutorAddr::fromPtr(&jitAtExit), 0,
              jitlink::Linkage::Strong, jitlink::Scope::Default, false);
          for(auto* B : G.blocks())
            for(auto& E : B->edges())
              if(&E.getTarget() == Sym)
                E.setTarget(atExitSym);
        }
      return Error::success();
    });
  }
  Error notifyFailed(MaterializationResponsibility& MR) override
  {
    errs() << "JITLink failed for " << MR.getTargetJITDylib().getName() << "\n";
    return Error::success();
  }
  Error notifyRemovingResources(JITDylib&, ResourceKey) override { return Error::success(); }
  void notifyTransferringResources(JITDylib&, ResourceKey, ResourceKey) override {}
private:
  MangleAndInterner& m_mangler;
};

class EagerLinkingPlugin : public ObjectLinkingLayer::Plugin
{
public:
  EagerLinkingPlugin(ExecutionSession& ES, MangleAndInterner& mangler) : ES{ES}, m_mangler{mangler} {}
  void modifyPassConfig(
      MaterializationResponsibility& MR, jitlink::LinkGraph&,
      jitlink::PassConfiguration& Config) override
  {
    Config.PreFixupPasses.push_back([this, &MR](jitlink::LinkGraph& G) -> Error {
      SymbolLookupSet ExternalSymbols;
      for(auto* Sym : G.external_symbols())
      {
        if((*Sym->getName()).starts_with("__lljit") || (*Sym->getName()).starts_with("___lljit"))
          continue;
        if(Sym->getName() == m_mangler("atexit"))
          continue;
        ExternalSymbols.add(ES.intern(*Sym->getName()));
      }
      if(!ExternalSymbols.empty())
      {
        auto SearchOrder = makeJITDylibSearchOrder(&MR.getTargetJITDylib());
        auto Result = ES.lookup(SearchOrder, ExternalSymbols);
        if(!Result)
          return Result.takeError();
      }
      return Error::success();
    });
  }
  Error notifyFailed(MaterializationResponsibility& MR) override
  {
    errs() << "JITLink failed for " << MR.getTargetJITDylib().getName() << "\n";
    return Error::success();
  }
  Error notifyRemovingResources(JITDylib&, ResourceKey) override { return Error::success(); }
  void notifyTransferringResources(JITDylib&, ResourceKey, ResourceKey) override {}
  ExecutionSession& ES;
  MangleAndInterner& m_mangler;
};
} // namespace

static std::atomic<int> g_objCounter{0};

int main(int argc, char** argv)
{
  if(argc < 2) { errs() << "usage: jit_repro <addon.bc>\n"; return 2; }

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();

  // Shared memory manager + mangler need to outlive the layer-creator lambda.
  auto MemMgr = std::move(*jitlink::InProcessMemoryManager::Create());
  std::unique_ptr<MangleAndInterner> Mangler;

  LLJITBuilder builder;
  auto JTMB = *JITTargetMachineBuilder::detectHost();
  JTMB.setCodeGenOptLevel(CodeGenOptLevel::Aggressive);
  setTargetOptions(JTMB.getOptions());
  outs() << "Target triple: " << JTMB.getTargetTriple().str() << "\n";
  builder.setJITTargetMachineBuilder(std::move(JTMB));
  builder.setNumCompileThreads(0); // single-threaded for deterministic dump

  builder.setObjectLinkingLayerCreator(
      [&](ExecutionSession& ES
#if LLVM_VERSION_MAJOR < 21
          , const Triple& TT
#endif
          ) {
    auto Layer = std::make_unique<ObjectLinkingLayer>(ES, *MemMgr);
#if LLVM_VERSION_MAJOR >= 21
    const Triple& TT = ES.getTargetTriple();
#endif
    if(TT.isOSBinFormatCOFF())
    {
      Layer->setOverrideObjectFlagsWithResponsibilityFlags(true);
      Layer->setAutoClaimResponsibilityForObjectSymbols(true);
    }
    // win-gnu x86_64 has an empty global prefix, so an empty DataLayout mangles
    // "atexit" -> "atexit" correctly; the real DL isn't available here yet.
    Mangler = std::make_unique<MangleAndInterner>(ES, DataLayout(""));
    Layer->addPlugin(std::make_shared<RuntimeInterposePlugin>(*Mangler));
    Layer->addPlugin(std::make_shared<EagerLinkingPlugin>(ES, *Mangler));
    return Layer;
  });

  auto JitOrErr = builder.create();
  if(!JitOrErr) { errs() << "LLJIT create failed: " << toString(JitOrErr.takeError()) << "\n"; return 1; }
  auto J = std::move(*JitOrErr);

  // Re-mangler against the real data layout (the one above used a placeholder).
  MangleAndInterner RealMangler{J->getExecutionSession(), J->getDataLayout()};

  // Dump every object ORC hands to JITLink, before linking.
  J->getObjTransformLayer().setTransform(
      [](std::unique_ptr<MemoryBuffer> Obj) -> Expected<std::unique_ptr<MemoryBuffer>> {
    int n = g_objCounter++;
    std::string name = "jit_obj_" + std::to_string(n) + ".o";
    std::error_code EC;
    raw_fd_ostream os(name, EC, sys::fs::OF_None);
    if(!EC) { os << Obj->getBuffer(); os.close(); outs() << "dumped " << name << " (" << Obj->getBufferSize() << " bytes)\n"; }
    return std::move(Obj);
  });

  J->getMainJITDylib().addGenerator(
      cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
          J->getDataLayout().getGlobalPrefix())));

  // Load the precompiled addon bitcode (exactly what score's clang produced).
  SMDiagnostic Err;
  auto Ctx = std::make_unique<LLVMContext>();
  auto M = parseIRFile(argv[1], Err, *Ctx);
  if(!M) { errs() << "parseIRFile failed: "; Err.print("jit_repro", errs()); return 1; }
  outs() << "module triple: " << M->getTargetTriple().str() << "\n";

  if(auto E = J->addIRModule(ThreadSafeModule(std::move(M), std::move(Ctx))))
  { errs() << "addIRModule failed: " << toString(std::move(E)) << "\n"; return 1; }

  // Force materialization (object emission + JITLink), like score's initialize().
  outs() << "initializing (forces JIT-link)...\n";
  if(auto E = J->initialize(J->getMainJITDylib()))
  {
    errs() << ">>> JIT ERROR: " << toString(std::move(E)) << "\n";
    return 0; // expected: this is what we are diagnosing
  }
  outs() << "initialize() succeeded (no error reproduced)\n";
  return 0;
}
