// Minimal JIT harness that shares score's MinGWCOFFPlatform.
//
// Links *only* LLVM ORC + JITLink + our MinGWCOFFPlatform -- no Qt, no score, no
// boost, no clang frontend. It JIT-loads a pre-compiled .ll/.bc module through the
// exact same platform + initialize() path the score JIT plugin uses, so we can
// isolate the Windows COFF static-init crash with a 5-line test instead of the
// full generated plugin.
//
//   usage: jitmin <orc_rt.a> <module.{ll,bc}> [entry-symbol]
//
// Every step is logged (flushed) so a hard crash leaves a trail pinpointing the
// phase (create / addIRModule / initialize / lookup / call).

#include <JitCpp/Compiler/MinGWCOFFPlatform.hpp>

#include <llvm/ExecutionEngine/JITLink/JITLinkMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/MapperJITLinkMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/MemoryMapper.h>
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <memory>

using namespace llvm;
using namespace llvm::orc;

// compiler-rt's emulated-TLS accessor (used by windows-gnu thread_local code).
extern "C" void* __emutls_get_address(void*);

static void step(const char* s)
{
  errs() << "[jitmin] " << s << "\n";
  errs().flush();
}

static void fail(const char* what, Error E)
{
  errs() << "[jitmin] FAIL " << what << ": " << toString(std::move(E)) << "\n";
  errs().flush();
  std::exit(2);
}

// Kept alive for the whole program: the ObjectLinkingLayer references it.
static std::unique_ptr<jitlink::JITLinkMemoryManager> g_memmgr;

int main(int argc, char** argv)
{
  if(argc < 3)
  {
    errs() << "usage: jitmin <orc_rt.a> <module.{ll,bc}> [entry]\n";
    return 1;
  }
  const char* orcRt = argv[1];
  const char* modPath = argv[2];
  const char* entry = argc > 3 ? argv[3] : "entry";

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  // One large contiguous slab so __ImageBase + every segment + the SEH unwind
  // sections (.pdata/.xdata) stay within 32-bit image-relative RVA range -- the
  // default scattering InProcessMemoryManager breaks real (large) add-ons.
  if(auto M = MapperJITLinkMemoryManager::CreateWithMapper<InProcessMemoryMapper>(
         size_t{1} << 30))
    g_memmgr = std::move(*M);
  else
    fail("MapperJITLinkMemoryManager::CreateWithMapper", M.takeError());

  LLJITBuilder builder;
  // Large code model: 64-bit references so vtable/type_info data pointers and far
  // host references don't get truncated by 32-bit absolute relocations at the
  // high JIT load address (mirrors the score JIT plugin).
  if(auto JTMB = JITTargetMachineBuilder::detectHost())
  {
    JTMB->setCodeModel(CodeModel::Large);
    builder.setJITTargetMachineBuilder(std::move(*JTMB));
  }
  else
    fail("JITTargetMachineBuilder::detectHost", JTMB.takeError());
  builder.setObjectLinkingLayerCreator(
      [](ExecutionSession& ES) -> Expected<std::unique_ptr<ObjectLayer>> {
    auto oll = std::make_unique<ObjectLinkingLayer>(ES, *g_memmgr);
    oll->setOverrideObjectFlagsWithResponsibilityFlags(true);
    oll->setAutoClaimResponsibilityForObjectSymbols(true);
    return oll;
  });

  builder.setPlatformSetUp(
      [orcRt](LLJIT& J) -> Expected<JITDylibSP> {
    auto ProcessSymbolsJD = J.getProcessSymbolsJITDylib();
    if(!ProcessSymbolsJD)
      return make_error<StringError>(
          "no process-symbols JITDylib", inconvertibleErrorCode());
    auto* OLL = dyn_cast<ObjectLinkingLayer>(&J.getObjLinkingLayer());
    if(!OLL)
      return make_error<StringError>(
          "need an ObjectLinkingLayer", inconvertibleErrorCode());
    auto& ES = J.getExecutionSession();
    auto& PlatformJD = ES.createBareJITDylib("<Platform>");
    PlatformJD.addToLinkOrder(*ProcessSymbolsJD);
    auto P = Jit::MinGWCOFFPlatform::Create(
        *OLL, PlatformJD, orcRt,
        [](JITDylib&, StringRef) -> Error { return Error::success(); });
    if(!P)
      return P.takeError();
    auto* PPtr = (*P).get();
    ES.setPlatform(std::move(*P));
    // Run add-on initializers directly instead of orc_rt's MSVC-coupled dlopen.
    J.setPlatformSupport(std::make_unique<Jit::MinGWCOFFPlatformSupport>(*PPtr));
    return &PlatformJD;
  });

  step("creating LLJIT + MinGWCOFFPlatform (orc_rt bootstrap)...");
  auto jit = builder.create();
  if(!jit)
    fail("LLJIT create / platform bootstrap", jit.takeError());
  step("LLJIT created OK");

  auto& JD = (*jit)->getMainJITDylib();
  if(auto G = DynamicLibrarySearchGenerator::GetForCurrentProcess(
         (*jit)->getDataLayout().getGlobalPrefix()))
    JD.addGenerator(std::move(*G));
  else
    fail("process-symbols generator", G.takeError());

  // Mirror Compiler.cpp: pin the cxxabi type_info vtables to the host libc++'s
  // complete copies (orc_rt's partial copy would give a JIT'd __vmi_class_type_info
  // a bad vtable and crash __dynamic_cast on cross-casts), and bind emulated-TLS's
  // __emutls_get_address. Done before any lookup materializes these symbols.
  {
    auto& ES = (*jit)->getExecutionSession();
    SymbolMap host;
    for(const char* sym :
        {"_ZTVN10__cxxabiv117__class_type_infoE",
         "_ZTVN10__cxxabiv120__si_class_type_infoE",
         "_ZTVN10__cxxabiv121__vmi_class_type_infoE",
         "_ZTVN10__cxxabiv117__pbase_type_infoE",
         "_ZTVN10__cxxabiv119__pointer_type_infoE",
         "_ZTVN10__cxxabiv129__pointer_to_member_type_infoE",
         "_ZTVN10__cxxabiv123__fundamental_type_infoE"})
      if(void* a = sys::DynamicLibrary::SearchForAddressOfSymbol(sym))
        host[ES.intern(sym)] = {ExecutorAddr::fromPtr(a), JITSymbolFlags::Exported};
    host[ES.intern("__emutls_get_address")]
        = {ExecutorAddr::fromPtr(&__emutls_get_address),
           JITSymbolFlags::Exported | JITSymbolFlags::Callable};
    if(auto E = JD.define(absoluteSymbols(std::move(host))))
      consumeError(std::move(E));
  }

  step("parsing module...");
  auto ctx = std::make_unique<LLVMContext>();
  SMDiagnostic diag;
  auto mod = parseIRFile(modPath, diag, *ctx);
  if(!mod)
  {
    errs() << "[jitmin] FAIL parse " << modPath << ": " << diag.getMessage()
           << "\n";
    return 2;
  }
  if(auto E = (*jit)->addIRModule(
         ThreadSafeModule(std::move(mod), std::move(ctx))))
    fail("addIRModule", std::move(E));
  step("module added");

  // Materialize first (the platform plugin collects the JD's initializers during
  // its post-fixup pass), THEN run initializers -- mirrors score's eager loop.
  step("lookup entry (materializes the module)...");
  auto sym = (*jit)->lookup(entry);
  if(!sym)
    fail("lookup", sym.takeError());

  if(std::getenv("SKIP_INIT"))
  {
    step("SKIP_INIT set -- not calling initialize()");
  }
  else
  {
    step("initialize() -- running static initializers...");
    if(auto E = (*jit)->initialize((*jit)->getMainJITDylib()))
      fail("initialize", std::move(E));
    step("initialize() OK");
  }

  step("calling entry...");
  auto fn = sym->toPtr<int()>();
  int r = fn();
  errs() << "[jitmin] entry returned " << r << "\n";
  errs() << "[jitmin] SUCCESS\n";
  errs().flush();
  return 0;
}
