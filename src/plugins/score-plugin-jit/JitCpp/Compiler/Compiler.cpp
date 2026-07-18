#include <JitCpp/Compiler/Compiler.hpp>

#include <ossia/detail/flat_map.hpp>

// Add JITLink-specific headers
#include <llvm/ExecutionEngine/JITLink/EHFrameSupport.h>
#include <llvm/ExecutionEngine/JITLink/JITLink.h>
#include <llvm/ExecutionEngine/Orc/EHFrameRegistrationPlugin.h>
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>

#if defined(_WIN32) && !defined(_MSC_VER) && LLVM_VERSION_MAJOR >= 22
#include <JitCpp/Compiler/MinGWCOFFPlatform.hpp>

#include <llvm/ExecutionEngine/Orc/COFF.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/MapperJITLinkMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/MemoryMapper.h>

#include <cstdlib>
#include <cstring>
#include <malloc.h>
#endif

#if LLVM_VERSION_MAJOR >= 22
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#endif

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

void setTargetOptions(
    llvm::TargetOptions& opts, bool useNativePlatform = false, bool isCOFF = false)
{
  // With an Orc Platform (orc_rt) in use we get native thread-locals on ELF/MachO;
  // without it the JIT has no TLS runtime, so fall back to emulated TLS.
  //
  // COFF is the exception: native Windows TLS needs a per-module _tls_index and a
  // registered .tls directory that only the CRT startup / loader provide -- the
  // JIT has neither, so a thread_local add-on fails to link ("Symbols not found:
  // _tls_index"). Force emulated TLS on COFF regardless of the platform;
  // __emutls_get_address then comes from the compiler-rt builtins archive we add
  // to the add-on's link order (see the platform setup below).
  opts.EmulatedTLS = !useNativePlatform || isCOFF;

  //opts.ExplicitEmulatedTLS = false;

#if LLVM_VERSION_MAJOR < 22
  opts.UnsafeFPMath = true;
  opts.ApproxFuncFPMath = true;
  opts.setFPDenormalMode(llvm::DenormalMode::getPositiveZero());
  opts.setFP32DenormalMode(llvm::DenormalMode::getPositiveZero());
#else
  opts.AllowFPOpFusion = llvm::FPOpFusion::Fast;
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
    qDebug() << "JITLink failed for " << MR.getTargetJITDylib().getName().c_str();
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

static std::unique_ptr<llvm::orc::LLJIT> jitBuilder(JitCompiler& self)
{
  // Make the host process's symbols searchable before building the JIT: the
  // MinGWCOFFPlatform bootstrap resolves host RTTI/vtable symbols through a
  // process-symbols generator during create(), below.
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
  SCORE_ASSERT(JTMB);

  llvm::orc::LLJITBuilder builder;

  JTMB->setCodeGenOptLevel(llvm::CodeGenOptLevel::Aggressive);

  // On Windows/COFF the add-on is JIT-mapped at a high address (the reserved
  // slab) and must reference host symbols far away -- libc++abi RTTI vtables and
  // the EH personality among them. The small (default) code model emits 32-bit
  // absolute relocations for those data pointers, which truncate at the high load
  // address and corrupt vtables / type_info, crashing __dynamic_cast during
  // plugin registration (and exception typeinfo lookups). The large code model
  // uses 64-bit references throughout, so cross-module RTTI and EH resolve.
  if(JTMB->getTargetTriple().isOSBinFormatCOFF())
    JTMB->setCodeModel(llvm::CodeModel::Large);

  // When the SDK ships LLVM's ORC runtime, drive the executor through an Orc
  // Platform (native TLS, real static-init/atexit and -- on COFF -- exception
  // registration). Otherwise keep the legacy path (emulated TLS + atexit
  // interpose + EH disabled).
  //
  // Platform selection (per target):
  //   ELF / MachO        : ExecutorNativePlatform(orc_rt)        (legacy path
  //                        as a fallback when orc_rt is absent or LLVM < 22)
  //   COFF + _MSC_VER    : ExecutorNativePlatform -> stock COFFPlatform, which
  //                        auto-discovers the MSVC VC runtime
  //   COFF + MinGW       : custom MinGWCOFFPlatform (COFFPlatform minus the VC
  //                        runtime bootstrap & _CxxThrowException alias)
  // On COFF the platform is mandatory: there is no working legacy COFF path, so
  // if orc_rt is missing on COFF (e.g. Windows-arm64) we refuse to set up JIT.
#if LLVM_VERSION_MAJOR >= 22
  const std::string orcRuntime = Jit::locateOrcRuntime();
#else
  const std::string orcRuntime;
#endif
  const bool useNativePlatform = !orcRuntime.empty();

  const bool isCOFF = JTMB->getTargetTriple().isOSBinFormatCOFF();
  if(isCOFF && !useNativePlatform)
  {
    // COFF requires the platform (TLS/SEH/initializers). Refuse rather than build
    // a broken JIT that would link COFF objects without exception/TLS support.
    // (throw, not SCORE_ASSERT: the latter is a no-op in release builds, which
    // would let create() proceed and fail later with a cryptic error.)
    throw std::runtime_error(
        "JIT: the SDK ships no orc_rt for this COFF target, which the JIT "
        "requires for TLS/SEH/initializers (e.g. none exists on Windows-arm64).");
  }

  self.m_useNativePlatform = useNativePlatform;

  setTargetOptions(JTMB->getOptions(), useNativePlatform, isCOFF);

  builder.setJITTargetMachineBuilder(std::move(*JTMB));
  builder.setNumCompileThreads(4);

  // Configure to use JITLink via ObjectLinkingLayer
  builder.setObjectLinkingLayerCreator(
      [&](llvm::orc::ExecutionSession& ES
            #if LLVM_VERSION_MAJOR < 21
              , const llvm::Triple& TT
            #endif
              ) {
    // Create ObjectLinkingLayer with JITLink
    auto ObjLinkingLayer
        = std::make_unique<llvm::orc::ObjectLinkingLayer>(ES, *self.m_memmgr);

    // COFF needs the same responsibility setup LLJIT applies to its own COFF
    // object layer; a hand-built ObjectLinkingLayer omits it, so JIT-linking COFF
    // objects fails with "Could not find symbol at given index, did you add it to
    // JITSymbolTable?" on symbols the layer never claimed.
#if LLVM_VERSION_MAJOR >= 21
    const llvm::Triple& TT = ES.getTargetTriple();
#endif
    if(TT.isOSBinFormatCOFF())
    {
      ObjLinkingLayer->setOverrideObjectFlagsWithResponsibilityFlags(true);
      ObjLinkingLayer->setAutoClaimResponsibilityForObjectSymbols(true);
    }

    // The atexit interpose and EH-frame handling are owned by the platform /
    // orc_rt when it is in use; only install our replacements on the legacy path.
    // (The eager-linking guarantee is enforced at add-on load time in compile(),
    // not via a JITLink pass -- see compile() -- so no eager plugin is needed.)
    if(!useNativePlatform)
    {
      ObjLinkingLayer->addPlugin(
          std::make_shared<RuntimeInterposePlugin>(self.m_mangler));

      // crash in deregisterEHFrames
      //ObjLinkingLayer->addPlugin(
      //    std::make_shared<llvm::orc::EHFrameRegistrationPlugin>(
      //        ES, std::make_unique<llvm::jitlink::InProcessEHFrameRegistrar>()));
    }

    return ObjLinkingLayer;
  });

#if LLVM_VERSION_MAJOR >= 22
  // Install the Orc Platform. It reuses the ObjectLinkingLayer created above and
  // adds its own generator/plugins; the process-symbols JITDylib it needs exists
  // by default (LinkProcessSymbolsByDefault).
  if(useNativePlatform)
  {
#if defined(_WIN32) && !defined(_MSC_VER)
    // MinGW COFF: stock COFFPlatform is MSVC-coupled (mandatory VC-runtime
    // bootstrap + _CxxThrowException alias). Use our VC-runtime-free subset; the
    // CRT/C++/EH symbols resolve from the host process (score.exe is MinGW-linked)
    // via the process-symbols generator. This mirrors ExecutorNativePlatform's
    // COFF setup (LLJIT.cpp) but swaps COFFPlatform for MinGWCOFFPlatform.
    builder.setPlatformSetUp(
        [orcRuntime](llvm::orc::LLJIT& J) -> llvm::Expected<llvm::orc::JITDylibSP> {
      auto ProcessSymbolsJD = J.getProcessSymbolsJITDylib();
      if(!ProcessSymbolsJD)
        return llvm::make_error<llvm::StringError>(
            "Native platforms require a process symbols JITDylib",
            llvm::inconvertibleErrorCode());

      auto* OLL
          = llvm::dyn_cast<llvm::orc::ObjectLinkingLayer>(&J.getObjLinkingLayer());
      if(!OLL)
        return llvm::make_error<llvm::StringError>(
            "MinGWCOFFPlatform requires an ObjectLinkingLayer",
            llvm::inconvertibleErrorCode());

      auto& ES = J.getExecutionSession();
      auto& PlatformJD = ES.createBareJITDylib("<Platform>");
      PlatformJD.addToLinkOrder(*ProcessSymbolsJD);

      // Pin the C allocator family to the exact functions score itself uses.
      // Third-party DLLs (e.g. JACK) pull the legacy msvcrt.dll into the
      // process, and it exports malloc/free too -- operating on a *different*
      // CRT heap. The process-symbols search is module-order dependent, so
      // JIT'd code could bind malloc to one CRT while the host frees with the
      // other; buffers do cross the JIT boundary (e.g. orc_rt's
      // WrapperFunctionResult error payloads, freed by the host in
      // runDeallocActions at teardown) and a mismatch corrupts the heap
      // (flaky 0xC0000374 at exit). Definitions beat the JD's process
      // generator, so every JIT resolution sees exactly these addresses.
      {
        llvm::orc::SymbolMap m;
        auto pin = [&](const char* n, auto* f) {
          m[ES.intern(n)]
              = {llvm::orc::ExecutorAddr::fromPtr(reinterpret_cast<void*>(f)),
                 llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable};
        };
        pin("malloc", &std::malloc);
        pin("free", &std::free);
        pin("calloc", &std::calloc);
        pin("realloc", &std::realloc);
        pin("_msize", &_msize);
        pin("_strdup", &_strdup);
        pin("_wcsdup", &_wcsdup);
        pin("_aligned_malloc", &_aligned_malloc);
        pin("_aligned_realloc", &_aligned_realloc);
        pin("_aligned_free", &_aligned_free);
        if(auto Err
           = ProcessSymbolsJD->define(llvm::orc::absoluteSymbols(std::move(m))))
          llvm::consumeError(std::move(Err));
      }

      // Provide compiler-rt's builtins in the add-on's link order (the driver
      // links these via -rtlib=compiler-rt; the bare -cc1 JIT compile does not, so
      // helpers it emits -- e.g. __udivti3, 128-bit division used by fmt -- are
      // otherwise unresolved). Attach on PlatformJD, next to orc_rt, so it is
      // materialized through the same platform path add-on link dependencies use.
      if(std::string builtins = Jit::locateBuiltinsRuntime(); !builtins.empty())
      {
        if(auto G = llvm::orc::StaticLibraryDefinitionGenerator::Load(
               *OLL, builtins.c_str()))
          PlatformJD.addGenerator(std::move(*G));
        else
          llvm::consumeError(G.takeError());
      }

      // Same for mingw-w64's libmingwex (the driver's implicit -lmingwex): the
      // C99-name math functions (hypotf, ...) the UCRT only exports under their
      // underscore names live there; its members call back into the host's
      // ucrtbase/kernel32 which resolve from the process as usual.
      if(std::string mingwex = Jit::locateMingwexRuntime(); !mingwex.empty())
      {
        if(auto G = llvm::orc::StaticLibraryDefinitionGenerator::Load(
               *OLL, mingwex.c_str()))
          PlatformJD.addGenerator(std::move(*G));
        else
          llvm::consumeError(G.takeError());
      }

      // And the UCRT import library (the driver's implicit -lucrt), in two
      // slices:
      //  - Its weak-external aliases (the POSIX old names: fileno -> _fileno)
      //    via COFFImportLibWeakAliasGenerator, which binds them to the same
      //    UCRT exports an AOT link would. (JITLink's COFF backend cannot load
      //    the alias objects themselves.) Added first so alias names never
      //    reach the static generator below.
      //  - Its real object members (e.g. __ms_fwprintf) via a
      //    StaticLibraryDefinitionGenerator; COFFImportFileScanner excludes the
      //    short import members, so __imp_* still resolves through the
      //    DLLImport generator against the already-loaded ucrtbase.
      if(std::string ucrt = Jit::locateUcrtImportLib(); !ucrt.empty())
      {
        if(auto G = Jit::COFFImportLibWeakAliasGenerator::Load(ucrt.c_str()))
          PlatformJD.addGenerator(std::move(*G));
        else
          llvm::consumeError(G.takeError());

        std::set<std::string> dylibs; // api-set DLLs; already loaded in-process
        if(auto G = llvm::orc::StaticLibraryDefinitionGenerator::Load(
               *OLL, ucrt.c_str(), llvm::orc::COFFImportFileScanner(dylibs)))
          PlatformJD.addGenerator(std::move(*G));
        else
          llvm::consumeError(G.takeError());
      }

      // libmingwex's _assert / iswctype wrappers call the CRT through *renamed*
      // imports (__imp___msvcrt_assert -> ucrt's _assert) that exist only inside
      // the import lib, so neither the process generator nor the DLLImport
      // generator can resolve them. Provide the import GOT slots directly:
      // host-allocated pointers filled with the ucrt export addresses -- exactly
      // the indirection an AOT link produces, so JIT'd code goes through the
      // same mingwex wrapper -> ucrt path as code linked by the driver.
      {
        auto& ES = J.getExecutionSession();
        // (import slot storage, __imp_ name, ucrt export it renames)
        static void* Slots[2];
        static constexpr std::pair<const char*, const char*> Renames[] = {
            {"__imp___msvcrt_assert", "_assert"},
            {"__imp___msvcrt_iswctype", "iswctype"},
        };
        llvm::orc::SymbolMap m;
        for(int i = 0; i < 2; i++)
          if((Slots[i] = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(
                  Renames[i].second)))
            m[ES.intern(Renames[i].first)]
                = {llvm::orc::ExecutorAddr::fromPtr(&Slots[i]),
                   llvm::JITSymbolFlags::Exported};
        if(!m.empty())
          if(auto Err = PlatformJD.define(llvm::orc::absoluteSymbols(std::move(m))))
            llvm::consumeError(std::move(Err));
      }

      auto P = Jit::MinGWCOFFPlatform::Create(
          *OLL, PlatformJD, orcRuntime.c_str(),
          [](llvm::orc::JITDylib& JD, llvm::StringRef DLLName) -> llvm::Error {
        // MinGW's orc_rt archive is statically self-contained and the host
        // process supplies CRT/C++/EH; there are no VC-runtime DLLs to preload.
        return llvm::Error::success();
      });
      if(!P)
        return P.takeError();
      auto* PPtr = (*P).get();
      ES.setPlatform(std::move(*P));
      // orc_rt's COFF dlopen init is MSVC-coupled (_initterm) and segfaults on
      // MinGW; run the add-on's static initializers directly instead.
      J.setPlatformSupport(
          std::make_unique<Jit::MinGWCOFFPlatformSupport>(*PPtr));
      return &PlatformJD;
    });
#else
    builder.setPlatformSetUp(llvm::orc::ExecutorNativePlatform(orcRuntime));
#endif
  }
#endif

  auto p = builder.create();
  SCORE_ASSERT(p);
  if(!p)
    qDebug() << toString(p.takeError()).c_str();
  SCORE_ASSERT(p.get());
  return std::move(p.get());
}

static std::unique_ptr<llvm::jitlink::JITLinkMemoryManager> makeMemoryManager()
{
#if defined(_WIN32) && !defined(_MSC_VER) && LLVM_VERSION_MAJOR >= 22
  // Windows x64 SEH unwind tables (.pdata/.xdata) reference code and unwind data
  // with 32-bit image-relative RVAs (target - __ImageBase). The default
  // InProcessMemoryManager hands out scattered mmap regions, so for a real add-on
  // the header and the code segments land >2GB apart and those RVAs overflow
  // ("relocation target out of range of Pointer32 fixup"). Reserve one large
  // contiguous slab instead, so __ImageBase + every segment stay within RVA range.
  constexpr size_t SlabSize = size_t{1} << 30; // 1 GiB reservation granularity
  auto m = llvm::orc::MapperJITLinkMemoryManager::CreateWithMapper<
      llvm::orc::InProcessMemoryMapper>(SlabSize);
  if(!m)
    throw std::runtime_error(
        "JIT: failed to create MapperJITLinkMemoryManager: "
        + toString(m.takeError()));
  return std::move(*m);
#else
  auto m = llvm::jitlink::InProcessMemoryManager::Create();
  if(!m)
    throw std::runtime_error(
        "JIT: failed to create InProcessMemoryManager: "
        + toString(m.takeError()));
  return std::move(*m);
#endif
}

JitCompiler::JitCompiler()
    : m_memmgr{makeMemoryManager()}
    , m_jit{jitBuilder(*this)}
    , m_mangler{m_jit->getExecutionSession(), m_jit->getDataLayout()}
{
  using namespace llvm;
  using namespace llvm::orc;
  // Load own executable as a dynamic library so the process-symbols generator can
  // resolve host symbols (CRT/C++/EH, score/ossia/Qt) back into JIT'd code.
  sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

  LLJIT& JIT = *m_jit;
  m_jit->getExecutionSession().setErrorReporter([&](llvm::Error Err) {
    llvm::handleAllErrors(std::move(Err), [&](const llvm::ErrorInfoBase& EI) {
      this->m_errors += EI.message().c_str();
    });
  });
  auto& JD = JIT.getMainJITDylib();

#if defined(_WIN32) && !defined(_MSC_VER) && LLVM_VERSION_MAJOR >= 22
  // Pin the cxxabi RTTI type_info vtables to the host libc++'s complete copies.
  //
  // On MinGW COFF the orc_rt archive (in the platform JD's link order) carries its
  // own statically-embedded slice of libc++abi, and during add-on symbol
  // resolution its copy of the type_info vtables wins over the host process's.
  // That copy is an incomplete placeholder (null method slots) for the
  // multiple-inheritance navigation, so a JIT'd class's __vmi_class_type_info ends
  // up with a bogus vtable pointer and __dynamic_cast segfaults on the first
  // cross-cast -- which is exactly what plugin registration does
  // (dynamic_cast<score::FactoryList_QtInterface*> etc. in registerPlugin()).
  //
  // Defining these symbols directly in the add-on's JITDylib makes them take
  // priority over the platform link order, so every JIT'd type_info points at the
  // host's real, complete cxxabi vtables. Only the vtables are pinned;
  // __dynamic_cast / __cxa_* still resolve from the process / orc_rt as usual.
  {
    auto& ES = JIT.getExecutionSession();
    SymbolMap host;
    for(const char* sym :
        {"_ZTVN10__cxxabiv117__class_type_infoE",
         "_ZTVN10__cxxabiv120__si_class_type_infoE",
         "_ZTVN10__cxxabiv121__vmi_class_type_infoE",
         "_ZTVN10__cxxabiv117__pbase_type_infoE",
         "_ZTVN10__cxxabiv119__pointer_type_infoE",
         "_ZTVN10__cxxabiv129__pointer_to_member_type_infoE",
         "_ZTVN10__cxxabiv123__fundamental_type_infoE"})
    {
      if(void* a = sys::DynamicLibrary::SearchForAddressOfSymbol(sym))
        host[ES.intern(sym)] = {ExecutorAddr::fromPtr(a), JITSymbolFlags::Exported};
    }
    if(!host.empty())
    {
      if(auto Err = JD.define(absoluteSymbols(std::move(host))))
        llvm::consumeError(std::move(Err));
    }
  }

  // Provide compiler-rt's builtins to the add-on. The driver links these via
  // -rtlib=compiler-rt, but the JIT compiles with bare -cc1, so runtime helpers it
  // emits -- e.g. __udivti3 (128-bit division used by fmt's formatting) -- are
  // unresolved and JIT load fails with "Symbols not found: __udivti3, ...". Hand
  // the builtins archive to ORC as a definition generator (same mechanism orc_rt
  // uses). Members are pulled on demand, so host-provided symbols still win.
#endif

  // When a platform is in use (LinkProcessSymbolsByDefault), LLJIT already builds
  // a <Process Symbols> JITDylib that is in the default link order, so adding a
  // second DynamicLibrarySearchGenerator here would be redundant and would reorder
  // symbol resolution. Only install it on the legacy (non-platform) path.
  if(!m_useNativePlatform)
  {
    auto gen = DynamicLibrarySearchGenerator::GetForCurrentProcess(
        m_jit->getDataLayout().getGlobalPrefix(),
        [&](const SymbolStringPtr& S) { return true; });
    if(!gen)
      throw std::runtime_error(
          "JIT: failed to create process-symbols generator: "
          + toString(gen.takeError()));
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

  // Names of all symbols the add-on defines; collected while the module is still
  // accessible so we can eagerly materialize them below.
  std::vector<std::string> definedSymbols;

#if LLVM_VERSION_MAJOR >= 21
  context.withContextDo([&](llvm::LLVMContext* c) {
#else
  {
    auto c = context.getContext();
#endif
    if(!c)
      throw std::runtime_error("Could not acquire an LLVMContext");
    auto module = m_driver.compileTranslationUnit(cppCode, flags, opts, *c);

    if(!module)
    {
      throw Exception{module.takeError()};
    }

    // Record every externally-visible definition so we can force-link the whole
    // add-on now, on this (the add-on-load) thread -- see the blocking lookup
    // below.
    for(const llvm::Function& F : (*module)->functions())
      if(!F.isDeclaration() && F.hasName() && !F.hasLocalLinkage())
        definedSymbols.push_back(F.getName().str());
    for(const llvm::GlobalVariable& G : (*module)->globals())
      if(!G.isDeclaration() && G.hasName() && !G.hasLocalLinkage()
         && !G.getName().starts_with("llvm."))
        definedSymbols.push_back(G.getName().str());

    if(auto Err = m_jit->addIRModule(ThreadSafeModule(std::move(*module), context));
       bool(Err))
    {
      throw Err;
    }
#if LLVM_VERSION_MAJOR >= 21
  });
#else
  }
#endif

  globalAtExit.currentCompiler = globalAtExit.nextCompilerID++;
  m_atExitId = globalAtExit.currentCompiler;

  // EAGER LINKING GUARANTEE:
  // The maintainer requires that no add-on function is ever JIT-compiled/linked
  // lazily on the real-time audio thread. We enforce this *here*, at add-on load
  // time on the load thread (NOT on a JITLink materialization thread, which would
  // deadlock against setNumCompileThreads(4) + platform bootstrap): a blocking
  // lookup of every symbol the add-on defines forces JITLink to compile and link
  // the entire add-on graph -- including resolving all of its external
  // dependencies -- before compile() returns. After this point everything the
  // add-on needs is already materialized, so getFunction()/the audio thread only
  // ever read an already-resolved address.
  for(const auto& name : definedSymbols)
  {
    // Don't fail the whole add-on for one un-resolvable helper symbol; record it
    // in m_errors and continue forcing the rest. The real entry-point lookup in
    // getFunction() still surfaces a hard error if it matters.
    if(auto sym = m_jit->lookup(name); !sym)
      consumeError(sym.takeError());
  }

  (void)m_jit->initialize(m_jit->getMainJITDylib());
}

}
