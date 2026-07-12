#include <JitCpp/Compiler/MinGWCOFFPlatform.hpp>

#if defined(_WIN32) && !defined(_MSC_VER) && LLVM_VERSION_MAJOR >= 22

#include <llvm/ExecutionEngine/JITLink/x86_64.h>
#include <llvm/ExecutionEngine/Orc/AbsoluteSymbols.h>
#include <llvm/ExecutionEngine/Orc/COFF.h>
#include <llvm/ExecutionEngine/Orc/LookupAndRecordAddrs.h>
#include <llvm/ExecutionEngine/Orc/ObjectFileInterface.h>
#include <llvm/ExecutionEngine/Orc/Shared/ObjectFormats.h>
#include <llvm/Object/COFF.h>
#include <llvm/Support/FormatVariadic.h>

using namespace llvm;
using namespace llvm::orc;
using namespace llvm::orc::shared;

namespace llvm
{
namespace orc
{
namespace shared
{
// SPS serializer types -- identical to the ones COFFPlatform.cpp declares
// privately in its TU; redeclared here because they are not exported.
using SPSCOFFJITDylibDepInfo = SPSSequence<SPSExecutorAddr>;
using SPSCOFFJITDylibDepInfoMap
    = SPSSequence<SPSTuple<SPSExecutorAddr, SPSCOFFJITDylibDepInfo>>;
using SPSCOFFObjectSectionsMap = SPSSequence<SPSTuple<SPSString, SPSExecutorAddrRange>>;
using SPSCOFFRegisterObjectSectionsArgs
    = SPSArgList<SPSExecutorAddr, SPSCOFFObjectSectionsMap, bool>;
using SPSCOFFDeregisterObjectSectionsArgs
    = SPSArgList<SPSExecutorAddr, SPSCOFFObjectSectionsMap>;
}
}
}

namespace
{

using Jit::MinGWCOFFPlatform;

// Defines the __ImageBase header symbol for a JITDylib. Identical to
// COFFPlatform's COFFHeaderMaterializationUnit but parameterised on our platform.
class COFFHeaderMaterializationUnit : public MaterializationUnit
{
public:
  COFFHeaderMaterializationUnit(
      MinGWCOFFPlatform& CP, const SymbolStringPtr& HeaderStartSymbol)
      : MaterializationUnit(createHeaderInterface(CP, HeaderStartSymbol))
      , CP(CP)
  {
  }

  StringRef getName() const override { return "MinGWCOFFHeaderMU"; }

  void materialize(std::unique_ptr<MaterializationResponsibility> R) override
  {
    auto G = std::make_unique<jitlink::LinkGraph>(
        "<COFFHeaderMU>", CP.getExecutionSession().getSymbolStringPool(),
        CP.getExecutionSession().getTargetTriple(), SubtargetFeatures(),
        jitlink::getGenericEdgeKindName);
    auto& HeaderSection = G->createSection("__header", MemProt::Read);
    auto& HeaderBlock = createHeaderBlock(*G, HeaderSection);

    // Init symbol is __ImageBase symbol.
    auto& ImageBaseSymbol = G->addDefinedSymbol(
        HeaderBlock, 0, *R->getInitializerSymbol(), HeaderBlock.getSize(),
        jitlink::Linkage::Strong, jitlink::Scope::Default, false, true);

    addImageBaseRelocationEdge(HeaderBlock, ImageBaseSymbol);

    CP.getObjectLinkingLayer().emit(std::move(R), std::move(G));
  }

  void discard(const JITDylib& JD, const SymbolStringPtr& Sym) override { }

private:
  struct NTHeader
  {
    support::ulittle32_t PEMagic;
    object::coff_file_header FileHeader;
    struct PEHeader
    {
      object::pe32plus_header Header;
      object::data_directory DataDirectory[COFF::NUM_DATA_DIRECTORIES + 1];
    } OptionalHeader;
  };

  struct HeaderBlockContent
  {
    object::dos_header DOSHeader;
    NTHeader NTHeader;
  };

  static jitlink::Block&
  createHeaderBlock(jitlink::LinkGraph& G, jitlink::Section& HeaderSection)
  {
    HeaderBlockContent Hdr = {};

    Hdr.DOSHeader.Magic[0] = 'M';
    Hdr.DOSHeader.Magic[1] = 'Z';
    Hdr.DOSHeader.AddressOfNewExeHeader = offsetof(HeaderBlockContent, NTHeader);
    uint32_t PEMagic = *reinterpret_cast<const uint32_t*>(COFF::PEMagic);
    Hdr.NTHeader.PEMagic = PEMagic;
    Hdr.NTHeader.OptionalHeader.Header.Magic = COFF::PE32Header::PE32_PLUS;

    switch(G.getTargetTriple().getArch())
    {
      case Triple::x86_64:
        Hdr.NTHeader.FileHeader.Machine = COFF::IMAGE_FILE_MACHINE_AMD64;
        break;
      default:
        llvm_unreachable("Unrecognized architecture");
    }

    auto HeaderContent = G.allocateContent(
        ArrayRef<char>(reinterpret_cast<const char*>(&Hdr), sizeof(Hdr)));

    return G.createContentBlock(HeaderSection, HeaderContent, ExecutorAddr(), 8, 0);
  }

  static void addImageBaseRelocationEdge(jitlink::Block& B, jitlink::Symbol& ImageBase)
  {
    auto ImageBaseOffset = offsetof(HeaderBlockContent, NTHeader)
                           + offsetof(NTHeader, OptionalHeader)
                           + offsetof(object::pe32plus_header, ImageBase);
    B.addEdge(jitlink::x86_64::Pointer64, ImageBaseOffset, ImageBase, 0);
  }

  static MaterializationUnit::Interface
  createHeaderInterface(MinGWCOFFPlatform& MOP, const SymbolStringPtr& HeaderStartSymbol)
  {
    SymbolFlagsMap HeaderSymbolFlags;
    HeaderSymbolFlags[HeaderStartSymbol] = JITSymbolFlags::Exported;
    return MaterializationUnit::Interface(
        std::move(HeaderSymbolFlags), HeaderStartSymbol);
  }

  MinGWCOFFPlatform& CP;
};

} // end anonymous namespace

namespace Jit
{

Expected<std::unique_ptr<MinGWCOFFPlatform>> MinGWCOFFPlatform::Create(
    ObjectLinkingLayer& ObjLinkingLayer, JITDylib& PlatformJD,
    std::unique_ptr<MemoryBuffer> OrcRuntimeArchiveBuffer,
    LoadDynamicLibrary LoadDynLibrary, std::optional<SymbolAliasMap> RuntimeAliases)
{
  auto& ES = ObjLinkingLayer.getExecutionSession();

  if(!supportedTarget(ES.getTargetTriple()))
    return make_error<StringError>(
        "Unsupported MinGWCOFFPlatform triple: " + ES.getTargetTriple().str(),
        inconvertibleErrorCode());

  auto& EPC = ES.getExecutorProcessControl();

  auto GeneratorArchive
      = object::Archive::create(OrcRuntimeArchiveBuffer->getMemBufferRef());
  if(!GeneratorArchive)
    return GeneratorArchive.takeError();

  std::set<std::string> DylibsToPreload;
  auto OrcRuntimeArchiveGenerator = StaticLibraryDefinitionGenerator::Create(
      ObjLinkingLayer, std::unique_ptr<MemoryBuffer>(nullptr),
      std::move(*GeneratorArchive), COFFImportFileScanner(DylibsToPreload));
  if(!OrcRuntimeArchiveGenerator)
    return OrcRuntimeArchiveGenerator.takeError();

  // Second instance of the archive for the platform itself.
  auto RuntimeArchive = cantFail(
      object::Archive::create(OrcRuntimeArchiveBuffer->getMemBufferRef()));

  if(!RuntimeAliases)
    RuntimeAliases = standardPlatformAliases(ES);

  if(auto Err = PlatformJD.define(symbolAliases(std::move(*RuntimeAliases))))
    return std::move(Err);

  auto& HostFuncJD = ES.createBareJITDylib("$<PlatformRuntimeHostFuncJD>");

  if(auto Err = HostFuncJD.define(absoluteSymbols(
         {{ES.intern("__orc_rt_jit_dispatch"),
           {EPC.getJITDispatchInfo().JITDispatchFunction, JITSymbolFlags::Exported}},
          {ES.intern("__orc_rt_jit_dispatch_ctx"),
           {EPC.getJITDispatchInfo().JITDispatchContext, JITSymbolFlags::Exported}}})))
    return std::move(Err);

  PlatformJD.addToLinkOrder(HostFuncJD);

  Error Err = Error::success();
  auto P = std::unique_ptr<MinGWCOFFPlatform>(new MinGWCOFFPlatform(
      ObjLinkingLayer, PlatformJD, std::move(*OrcRuntimeArchiveGenerator),
      std::move(DylibsToPreload), std::move(OrcRuntimeArchiveBuffer),
      std::move(RuntimeArchive), std::move(LoadDynLibrary), Err));
  if(Err)
    return std::move(Err);
  return std::move(P);
}

Expected<std::unique_ptr<MinGWCOFFPlatform>> MinGWCOFFPlatform::Create(
    ObjectLinkingLayer& ObjLinkingLayer, JITDylib& PlatformJD,
    const char* OrcRuntimePath, LoadDynamicLibrary LoadDynLibrary,
    std::optional<SymbolAliasMap> RuntimeAliases)
{
  auto ArchiveBuffer = MemoryBuffer::getFile(OrcRuntimePath);
  if(!ArchiveBuffer)
    return createFileError(OrcRuntimePath, ArchiveBuffer.getError());

  return Create(
      ObjLinkingLayer, PlatformJD, std::move(*ArchiveBuffer), std::move(LoadDynLibrary),
      std::move(RuntimeAliases));
}

Expected<MemoryBufferRef> MinGWCOFFPlatform::getPerJDObjectFile()
{
  auto PerJDObj = OrcRuntimeArchive->findSym("__orc_rt_coff_per_jd_marker");
  if(!PerJDObj)
    return PerJDObj.takeError();

  if(!*PerJDObj)
    return make_error<StringError>(
        "Could not find per jd object file", inconvertibleErrorCode());

  auto Buffer = (*PerJDObj)->getAsBinary();
  if(!Buffer)
    return Buffer.takeError();

  return (*Buffer)->getMemoryBufferRef();
}

static void addAliases(
    ExecutionSession& ES, SymbolAliasMap& Aliases,
    ArrayRef<std::pair<const char*, const char*>> AL)
{
  for(auto& KV : AL)
  {
    auto AliasName = ES.intern(KV.first);
    assert(!Aliases.count(AliasName) && "Duplicate symbol name in alias map");
    Aliases[std::move(AliasName)] = {ES.intern(KV.second), JITSymbolFlags::Exported};
  }
}

Error MinGWCOFFPlatform::setupJITDylib(JITDylib& JD)
{
  if(auto Err = JD.define(
         std::make_unique<COFFHeaderMaterializationUnit>(*this, COFFHeaderStartSymbol)))
    return Err;

  if(auto Err = ES.lookup({&JD}, COFFHeaderStartSymbol).takeError())
    return Err;

  // Define the CXX/atexit aliases (NO _CxxThrowException -- see header).
  SymbolAliasMap CXXAliases;
  addAliases(ES, CXXAliases, requiredCXXAliases());
  if(auto Err = JD.define(symbolAliases(std::move(CXXAliases))))
    return Err;

  auto PerJDObj = getPerJDObjectFile();
  if(!PerJDObj)
    return PerJDObj.takeError();

  auto I = getObjectFileInterface(ES, *PerJDObj);
  if(!I)
    return I.takeError();

  if(auto Err = ObjLinkingLayer.add(
         JD, MemoryBuffer::getMemBuffer(*PerJDObj, false), std::move(*I)))
    return Err;

  // No VC-runtime load here: on MinGW the CRT/C++/EH symbols are resolved from
  // the host process (score.exe) via the process-symbols generator.

  JD.addGenerator(DLLImportDefinitionGenerator::Create(ES, ObjLinkingLayer));
  return Error::success();
}

Error MinGWCOFFPlatform::teardownJITDylib(JITDylib& JD)
{
  std::lock_guard<std::mutex> Lock(PlatformMutex);
  auto I = JITDylibToHeaderAddr.find(&JD);
  if(I != JITDylibToHeaderAddr.end())
  {
    assert(
        HeaderAddrToJITDylib.count(I->second) && "HeaderAddrToJITDylib missing entry");
    HeaderAddrToJITDylib.erase(I->second);
    JITDylibToHeaderAddr.erase(I);
  }
  return Error::success();
}

Error MinGWCOFFPlatform::notifyAdding(
    ResourceTracker& RT, const MaterializationUnit& MU)
{
  auto& JD = RT.getJITDylib();
  const auto& InitSym = MU.getInitializerSymbol();
  if(!InitSym)
    return Error::success();

  RegisteredInitSymbols[&JD].add(InitSym, SymbolLookupFlags::WeaklyReferencedSymbol);
  return Error::success();
}

Error MinGWCOFFPlatform::notifyRemoving(ResourceTracker& RT)
{
  llvm_unreachable("Not supported yet");
}

SymbolAliasMap MinGWCOFFPlatform::standardPlatformAliases(ExecutionSession& ES)
{
  SymbolAliasMap Aliases;
  addAliases(ES, Aliases, standardRuntimeUtilityAliases());
  return Aliases;
}

ArrayRef<std::pair<const char*, const char*>> MinGWCOFFPlatform::requiredCXXAliases()
{
  // NB: _CxxThrowException is intentionally absent (MSVC-only). MinGW exceptions
  // use libunwind / __gxx_personality_seh0, resolved from the host process.
  static const std::pair<const char*, const char*> RequiredCXXAliases[]
      = {{"_onexit", "__orc_rt_coff_onexit_per_jd"},
         {"atexit", "__orc_rt_coff_atexit_per_jd"}};

  return ArrayRef<std::pair<const char*, const char*>>(RequiredCXXAliases);
}

ArrayRef<std::pair<const char*, const char*>>
MinGWCOFFPlatform::standardRuntimeUtilityAliases()
{
  static const std::pair<const char*, const char*> StandardRuntimeUtilityAliases[]
      = {{"__orc_rt_run_program", "__orc_rt_coff_run_program"},
         {"__orc_rt_jit_dlerror", "__orc_rt_coff_jit_dlerror"},
         {"__orc_rt_jit_dlopen", "__orc_rt_coff_jit_dlopen"},
         {"__orc_rt_jit_dlupdate", "__orc_rt_coff_jit_dlupdate"},
         {"__orc_rt_jit_dlclose", "__orc_rt_coff_jit_dlclose"},
         {"__orc_rt_jit_dlsym", "__orc_rt_coff_jit_dlsym"},
         {"__orc_rt_log_error", "__orc_rt_log_error_to_stderr"}};

  return ArrayRef<std::pair<const char*, const char*>>(StandardRuntimeUtilityAliases);
}

bool MinGWCOFFPlatform::supportedTarget(const Triple& TT)
{
  switch(TT.getArch())
  {
    case Triple::x86_64:
      return true;
    default:
      return false;
  }
}

MinGWCOFFPlatform::MinGWCOFFPlatform(
    ObjectLinkingLayer& ObjLinkingLayer, JITDylib& PlatformJD,
    std::unique_ptr<StaticLibraryDefinitionGenerator> OrcRuntimeGenerator,
    std::set<std::string> DylibsToPreload,
    std::unique_ptr<MemoryBuffer> OrcRuntimeArchiveBuffer,
    std::unique_ptr<object::Archive> OrcRuntimeArchive, LoadDynamicLibrary LoadDynLibrary,
    Error& Err)
    : ES(ObjLinkingLayer.getExecutionSession())
    , ObjLinkingLayer(ObjLinkingLayer)
    , LoadDynLibrary(std::move(LoadDynLibrary))
    , OrcRuntimeArchiveBuffer(std::move(OrcRuntimeArchiveBuffer))
    , OrcRuntimeArchive(std::move(OrcRuntimeArchive))
    , COFFHeaderStartSymbol(ES.intern("__ImageBase"))
{
  ErrorAsOutParameter _(Err);

  Bootstrapping.store(true);
  ObjLinkingLayer.addPlugin(std::make_unique<MinGWCOFFPlatformPlugin>(*this));

  // No VC-runtime bootstrap. Just expose the orc_rt archive to PlatformJD.
  PlatformJD.addGenerator(std::move(OrcRuntimeGenerator));

  if(auto E2 = setupJITDylib(PlatformJD))
  {
    Err = std::move(E2);
    return;
  }

  for(auto& Lib : DylibsToPreload)
    if(auto E2 = this->LoadDynLibrary(PlatformJD, Lib))
    {
      Err = std::move(E2);
      return;
    }

  if(auto E2 = associateRuntimeSupportFunctions(PlatformJD))
  {
    Err = std::move(E2);
    return;
  }

  if(auto E2 = bootstrapCOFFRuntime(PlatformJD))
  {
    Err = std::move(E2);
    return;
  }

  Bootstrapping.store(false);
  JDBootstrapStates.clear();
}

Expected<MinGWCOFFPlatform::JITDylibDepMap>
MinGWCOFFPlatform::buildJDDepMap(JITDylib& JD)
{
  return ES.runSessionLocked([&]() -> Expected<JITDylibDepMap> {
    JITDylibDepMap JDDepMap;

    SmallVector<JITDylib*, 16> Worklist({&JD});
    while(!Worklist.empty())
    {
      auto CurJD = Worklist.back();
      Worklist.pop_back();

      auto& DM = JDDepMap[CurJD];
      CurJD->withLinkOrderDo([&](const JITDylibSearchOrder& O) {
        DM.reserve(O.size());
        for(auto& KV : O)
        {
          if(KV.first == CurJD)
            continue;
          {
            std::lock_guard<std::mutex> Lock(PlatformMutex);
            if(!JITDylibToHeaderAddr.count(KV.first))
              continue;
          }
          DM.push_back(KV.first);
          if(JDDepMap.try_emplace(KV.first).second)
            Worklist.push_back(KV.first);
        }
      });
    }
    return std::move(JDDepMap);
  });
}

void MinGWCOFFPlatform::pushInitializersLoop(
    PushInitializersSendResultFn SendResult, JITDylibSP JD, JITDylibDepMap& JDDepMap)
{
  SmallVector<JITDylib*, 16> Worklist({JD.get()});
  DenseSet<JITDylib*> Visited({JD.get()});
  DenseMap<JITDylib*, SymbolLookupSet> NewInitSymbols;
  ES.runSessionLocked([&]() {
    while(!Worklist.empty())
    {
      auto CurJD = Worklist.back();
      Worklist.pop_back();

      auto RISItr = RegisteredInitSymbols.find(CurJD);
      if(RISItr != RegisteredInitSymbols.end())
      {
        NewInitSymbols[CurJD] = std::move(RISItr->second);
        RegisteredInitSymbols.erase(RISItr);
      }

      for(auto* DepJD : JDDepMap[CurJD])
        if(Visited.insert(DepJD).second)
          Worklist.push_back(DepJD);
    }
  });

  if(NewInitSymbols.empty())
  {
    COFFJITDylibDepInfoMap DIM;
    DIM.reserve(JDDepMap.size());
    for(auto& KV : JDDepMap)
    {
      std::lock_guard<std::mutex> Lock(PlatformMutex);
      COFFJITDylibDepInfo DepInfo;
      DepInfo.reserve(KV.second.size());
      for(auto& Dep : KV.second)
        DepInfo.push_back(JITDylibToHeaderAddr[Dep]);
      auto H = JITDylibToHeaderAddr[KV.first];
      DIM.push_back(std::make_pair(H, std::move(DepInfo)));
    }
    SendResult(DIM);
    return;
  }

  lookupInitSymbolsAsync(
      [this, SendResult = std::move(SendResult), &JD,
       JDDepMap = std::move(JDDepMap)](Error Err) mutable {
    if(Err)
      SendResult(std::move(Err));
    else
      pushInitializersLoop(std::move(SendResult), JD, JDDepMap);
  },
      ES, std::move(NewInitSymbols));
}

void MinGWCOFFPlatform::rt_pushInitializers(
    PushInitializersSendResultFn SendResult, ExecutorAddr JDHeaderAddr)
{
  JITDylibSP JD;
  {
    std::lock_guard<std::mutex> Lock(PlatformMutex);
    auto I = HeaderAddrToJITDylib.find(JDHeaderAddr);
    if(I != HeaderAddrToJITDylib.end())
      JD = I->second;
  }

  if(!JD)
  {
    SendResult(make_error<StringError>(
        "No JITDylib with header addr " + formatv("{0:x}", JDHeaderAddr).str(),
        inconvertibleErrorCode()));
    return;
  }

  auto JDDepMap = buildJDDepMap(*JD);
  if(!JDDepMap)
  {
    SendResult(JDDepMap.takeError());
    return;
  }
  pushInitializersLoop(std::move(SendResult), JD, *JDDepMap);
}

void MinGWCOFFPlatform::rt_lookupSymbol(
    SendSymbolAddressFn SendResult, ExecutorAddr Handle, StringRef SymbolName)
{
  JITDylib* JD = nullptr;

  {
    std::lock_guard<std::mutex> Lock(PlatformMutex);
    auto I = HeaderAddrToJITDylib.find(Handle);
    if(I != HeaderAddrToJITDylib.end())
      JD = I->second;
  }

  if(!JD)
  {
    SendResult(make_error<StringError>(
        "No JITDylib associated with handle " + formatv("{0:x}", Handle).str(),
        inconvertibleErrorCode()));
    return;
  }

  class RtLookupNotifyComplete
  {
  public:
    RtLookupNotifyComplete(SendSymbolAddressFn&& SendResult)
        : SendResult(std::move(SendResult))
    {
    }
    void operator()(Expected<SymbolMap> Result)
    {
      if(Result)
      {
        assert(Result->size() == 1 && "Unexpected result map count");
        SendResult(Result->begin()->second.getAddress());
      }
      else
      {
        SendResult(Result.takeError());
      }
    }

  private:
    SendSymbolAddressFn SendResult;
  };

  ES.lookup(
      LookupKind::DLSym, {{JD, JITDylibLookupFlags::MatchExportedSymbolsOnly}},
      SymbolLookupSet(ES.intern(SymbolName)), SymbolState::Ready,
      RtLookupNotifyComplete(std::move(SendResult)), NoDependenciesToRegister);
}

Error MinGWCOFFPlatform::associateRuntimeSupportFunctions(JITDylib& PlatformJD)
{
  ExecutionSession::JITDispatchHandlerAssociationMap WFs;

  using LookupSymbolSPSSig = SPSExpected<SPSExecutorAddr>(SPSExecutorAddr, SPSString);
  WFs[ES.intern("__orc_rt_coff_symbol_lookup_tag")]
      = ES.wrapAsyncWithSPS<LookupSymbolSPSSig>(
          this, &MinGWCOFFPlatform::rt_lookupSymbol);
  using PushInitializersSPSSig = SPSExpected<SPSCOFFJITDylibDepInfoMap>(SPSExecutorAddr);
  WFs[ES.intern("__orc_rt_coff_push_initializers_tag")]
      = ES.wrapAsyncWithSPS<PushInitializersSPSSig>(
          this, &MinGWCOFFPlatform::rt_pushInitializers);

  return ES.registerJITDispatchHandlers(PlatformJD, std::move(WFs));
}

Error MinGWCOFFPlatform::runBootstrapInitializers(JDBootstrapState& BState)
{
  llvm::sort(BState.Initializers);
  if(auto Err = runBootstrapSubsectionInitializers(BState, ".CRT$XIA", ".CRT$XIZ"))
    return Err;

  if(auto Err = runSymbolIfExists(*BState.JD, "__run_after_c_init"))
    return Err;

  if(auto Err = runBootstrapSubsectionInitializers(BState, ".CRT$XCA", ".CRT$XCZ"))
    return Err;
  return Error::success();
}

Error MinGWCOFFPlatform::runBootstrapSubsectionInitializers(
    JDBootstrapState& BState, StringRef Start, StringRef End)
{
  for(auto& Initializer : BState.Initializers)
    if(Initializer.first >= Start && Initializer.first <= End && Initializer.second)
    {
      auto Res = ES.getExecutorProcessControl().runAsVoidFunction(Initializer.second);
      if(!Res)
        return Res.takeError();
    }
  return Error::success();
}

Error MinGWCOFFPlatform::runJDInitializers(JITDylib& JD)
{
  SmallVector<std::pair<std::string, ExecutorAddr>, 16> Inits;
  {
    std::lock_guard<std::mutex> Lock(PlatformMutex);
    auto It = JDInitializers.find(&JD);
    if(It != JDInitializers.end())
      Inits.assign(It->second.begin(), It->second.end());
  }
  llvm::sort(Inits);

  auto runIf = [&](auto pred) -> Error {
    for(auto& I : Inits)
      if(I.second && pred(StringRef(I.first)))
      {
        auto Res = ES.getExecutorProcessControl().runAsVoidFunction(I.second);
        if(!Res)
          return Res.takeError();
      }
    return Error::success();
  };
  // C init (.CRT$XI*), C++ ctors (.CRT$XC*), then GNU-style .init_array / .ctors
  // (windows-gnu may emit those instead of the MSVC .CRT$XC* section).
  if(auto E = runIf([](StringRef n) { return n >= ".CRT$XIA" && n <= ".CRT$XIZ"; }))
    return E;
  if(auto E = runIf([](StringRef n) { return n >= ".CRT$XCA" && n <= ".CRT$XCZ"; }))
    return E;
  if(auto E = runIf([](StringRef n) { return n.starts_with(".init_array"); }))
    return E;
  // Scraped init functions ($.<module>.ll.__inits.N, stored under ".jit$init")
  // and GNU-style .ctors come last.
  if(auto E = runIf([](StringRef n) { return n == ".jit$init"; }))
    return E;
  return runIf([](StringRef n) { return n.starts_with(".ctors"); });
}

Error MinGWCOFFPlatform::initializeJITDylib(JITDylib& JD)
{
  {
    std::lock_guard<std::mutex> Lock(PlatformMutex);
    if(!InitializedJDs.insert(&JD).second)
      return Error::success(); // already initialized
  }
  return runJDInitializers(JD);
}

Error MinGWCOFFPlatform::bootstrapCOFFRuntime(JITDylib& PlatformJD)
{
  if(auto Err = lookupAndRecordAddrs(
         ES, LookupKind::Static, makeJITDylibSearchOrder(&PlatformJD),
         {
             {ES.intern("__orc_rt_coff_platform_bootstrap"),
              &orc_rt_coff_platform_bootstrap},
             {ES.intern("__orc_rt_coff_platform_shutdown"),
              &orc_rt_coff_platform_shutdown},
             {ES.intern("__orc_rt_coff_register_jitdylib"),
              &orc_rt_coff_register_jitdylib},
             {ES.intern("__orc_rt_coff_deregister_jitdylib"),
              &orc_rt_coff_deregister_jitdylib},
             {ES.intern("__orc_rt_coff_register_object_sections"),
              &orc_rt_coff_register_object_sections},
             {ES.intern("__orc_rt_coff_deregister_object_sections"),
              &orc_rt_coff_deregister_object_sections},
         }))
    return Err;

  if(auto Err = ES.callSPSWrapper<void()>(orc_rt_coff_platform_bootstrap))
    return Err;

  // Do the pending jitdylib registration actions that we couldn't do
  // because orc runtime was not linked fully.
  for(auto KV : JDBootstrapStates)
  {
    auto& JDBState = KV.second;
    if(auto Err = ES.callSPSWrapper<void(SPSString, SPSExecutorAddr)>(
           orc_rt_coff_register_jitdylib, JDBState.JDName, JDBState.HeaderAddr))
      return Err;

    for(auto& ObjSectionMap : JDBState.ObjectSectionsMaps)
      if(auto Err
         = ES.callSPSWrapper<void(SPSExecutorAddr, SPSCOFFObjectSectionsMap, bool)>(
             orc_rt_coff_register_object_sections, JDBState.HeaderAddr, ObjSectionMap,
             false))
        return Err;
  }

  for(auto KV : JDBootstrapStates)
  {
    auto& JDBState = KV.second;
    if(auto Err = runBootstrapInitializers(JDBState))
      return Err;
  }

  return Error::success();
}

Error MinGWCOFFPlatform::runSymbolIfExists(JITDylib& PlatformJD, StringRef SymbolName)
{
  ExecutorAddr jit_function;
  auto AfterCLookupErr = lookupAndRecordAddrs(
      ES, LookupKind::Static, makeJITDylibSearchOrder(&PlatformJD),
      {{ES.intern(SymbolName), &jit_function}});
  if(!AfterCLookupErr)
  {
    auto Res = ES.getExecutorProcessControl().runAsVoidFunction(jit_function);
    if(!Res)
      return Res.takeError();
    return Error::success();
  }
  if(!AfterCLookupErr.isA<SymbolsNotFound>())
    return AfterCLookupErr;
  consumeError(std::move(AfterCLookupErr));
  return Error::success();
}

void MinGWCOFFPlatform::MinGWCOFFPlatformPlugin::modifyPassConfig(
    MaterializationResponsibility& MR, jitlink::LinkGraph& LG,
    jitlink::PassConfiguration& Config)
{
  bool IsBootstrapping = CP.Bootstrapping.load();

  if(auto InitSymbol = MR.getInitializerSymbol())
  {
    if(InitSymbol == CP.COFFHeaderStartSymbol)
    {
      Config.PostAllocationPasses.push_back(
          [this, &MR, IsBootstrapping](jitlink::LinkGraph& G) {
        return associateJITDylibHeaderSymbol(G, MR, IsBootstrapping);
      });
      return;
    }
    Config.PrePrunePasses.push_back([this, &MR](jitlink::LinkGraph& G) {
      return preserveInitializerSections(G, MR);
    });

  }

  if(!IsBootstrapping)
    Config.PostFixupPasses.push_back(
        [this, &JD = MR.getTargetJITDylib()](jitlink::LinkGraph& G) {
      return registerObjectPlatformSections(G, JD);
    });
  else
    Config.PostFixupPasses.push_back(
        [this, &JD = MR.getTargetJITDylib()](jitlink::LinkGraph& G) {
      return registerObjectPlatformSectionsInBootstrap(G, JD);
    });
}

Error MinGWCOFFPlatform::MinGWCOFFPlatformPlugin::associateJITDylibHeaderSymbol(
    jitlink::LinkGraph& G, MaterializationResponsibility& MR, bool IsBootstraping)
{
  auto I = llvm::find_if(G.defined_symbols(), [this](jitlink::Symbol* Sym) {
    return *Sym->getName() == *CP.COFFHeaderStartSymbol;
  });
  assert(I != G.defined_symbols().end() && "Missing COFF header start symbol");

  auto& JD = MR.getTargetJITDylib();
  std::lock_guard<std::mutex> Lock(CP.PlatformMutex);
  auto HeaderAddr = (*I)->getAddress();
  CP.JITDylibToHeaderAddr[&JD] = HeaderAddr;
  CP.HeaderAddrToJITDylib[HeaderAddr] = &JD;
  if(!IsBootstraping)
  {
    G.allocActions().push_back(
        {cantFail(WrapperFunctionCall::Create<SPSArgList<SPSString, SPSExecutorAddr>>(
             CP.orc_rt_coff_register_jitdylib, JD.getName(), HeaderAddr)),
         cantFail(WrapperFunctionCall::Create<SPSArgList<SPSExecutorAddr>>(
             CP.orc_rt_coff_deregister_jitdylib, HeaderAddr))});
  }
  else
  {
    G.allocActions().push_back(
        {{},
         cantFail(WrapperFunctionCall::Create<SPSArgList<SPSExecutorAddr>>(
             CP.orc_rt_coff_deregister_jitdylib, HeaderAddr))});
    JDBootstrapState BState;
    BState.JD = &JD;
    BState.JDName = JD.getName();
    BState.HeaderAddr = HeaderAddr;
    CP.JDBootstrapStates.emplace(&JD, BState);
  }

  return Error::success();
}

Error MinGWCOFFPlatform::MinGWCOFFPlatformPlugin::registerObjectPlatformSections(
    jitlink::LinkGraph& G, JITDylib& JD)
{
  COFFObjectSectionsMap ObjSecs;
  auto HeaderAddr = CP.JITDylibToHeaderAddr[&JD];
  assert(HeaderAddr && "Must be registered jitdylib");
  for(auto& S : G.sections())
  {
    jitlink::SectionRange Range(S);
    if(Range.getSize())
      ObjSecs.push_back(std::make_pair(S.getName().str(), Range.getRange()));
  }

  G.allocActions().push_back(
      {cantFail(WrapperFunctionCall::Create<SPSCOFFRegisterObjectSectionsArgs>(
           CP.orc_rt_coff_register_object_sections, HeaderAddr, ObjSecs, true)),
       cantFail(WrapperFunctionCall::Create<SPSCOFFDeregisterObjectSectionsArgs>(
           CP.orc_rt_coff_deregister_object_sections, HeaderAddr, ObjSecs))});

  // Collect this object's MSVC-style (.CRT$X*) and GNU-style (.ctors/.init_array)
  // static initializer section edges, so MinGWCOFFPlatformSupport can run them
  // directly (orc_rt's COFF dlopen init is MSVC-coupled and faults on MinGW).
  // Most IR add-ons have none of these (the ctors live in __cxx_global_var_init,
  // collected from the link graph by the init-extract post-fixup pass instead).
  {
    std::lock_guard<std::mutex> Lock(CP.PlatformMutex);
    auto& Inits = CP.JDInitializers[&JD];
    size_t before = Inits.size();
    for(auto& S : G.sections())
    {
      StringRef N = S.getName();
      if(isCOFFInitializerSection(N) || N.starts_with(".ctors")
         || N.starts_with(".init_array"))
        for(auto* B : S.blocks())
          for(auto& E : B->edges())
            Inits.push_back(std::make_pair(
                N.str(), E.getTarget().getAddress() + E.getAddend()));
    }

    // Fallback for pre-scraped IR modules (e.g. addIRModule of a .ll): LLJIT's
    // IR scraper consumed llvm.global_ctors into a body-less side-effect symbol
    // and left .ctors empty, so the loop above found nothing. The runnable init
    // code is then clang's entry point -- _GLOBAL__sub_I_<tu> (chains every
    // __cxx_global_var_init in order) if present, else the lone
    // __cxx_global_var_init. Only used when no init-section edges exist, so we
    // never double-run the constructors collected from .ctors/.CRT$XC* above.
    if(Inits.size() == before)
    {
      SmallVector<ExecutorAddr, 4> SubI, CxxInit;
      for(auto* Sym : G.defined_symbols())
      {
        if(!Sym->hasName())
          continue;
        StringRef N = *Sym->getName();
        if(N.starts_with("_GLOBAL__sub_I_"))
          SubI.push_back(Sym->getAddress());
        else if(N == "__cxx_global_var_init"
                || N.starts_with("__cxx_global_var_init."))
          CxxInit.push_back(Sym->getAddress());
      }
      for(auto A : (!SubI.empty() ? SubI : CxxInit))
        Inits.push_back(std::make_pair(std::string(".jit$init"), A));
    }
  }

  return Error::success();
}

Error MinGWCOFFPlatform::MinGWCOFFPlatformPlugin::preserveInitializerSections(
    jitlink::LinkGraph& G, MaterializationResponsibility& MR)
{
  if(const auto& InitSymName = MR.getInitializerSymbol())
  {
    jitlink::Symbol* InitSym = nullptr;

    for(auto& InitSection : G.sections())
    {
      if(!isCOFFInitializerSection(InitSection.getName()) || InitSection.empty())
        continue;

      if(!InitSym)
      {
        auto& B = **InitSection.blocks().begin();
        InitSym = &G.addDefinedSymbol(
            B, 0, *InitSymName, B.getSize(), jitlink::Linkage::Strong,
            jitlink::Scope::SideEffectsOnly, false, true);
      }

      for(auto* B : InitSection.blocks())
      {
        if(B == &InitSym->getBlock())
          continue;

        auto& S = G.addAnonymousSymbol(*B, 0, B->getSize(), false, true);
        InitSym->getBlock().addEdge(jitlink::Edge::KeepAlive, 0, S, 0);
      }
    }
  }

  return Error::success();
}

Error MinGWCOFFPlatform::MinGWCOFFPlatformPlugin::
    registerObjectPlatformSectionsInBootstrap(jitlink::LinkGraph& G, JITDylib& JD)
{
  std::lock_guard<std::mutex> Lock(CP.PlatformMutex);
  auto HeaderAddr = CP.JITDylibToHeaderAddr[&JD];
  COFFObjectSectionsMap ObjSecs;
  for(auto& S : G.sections())
  {
    jitlink::SectionRange Range(S);
    if(Range.getSize())
      ObjSecs.push_back(std::make_pair(S.getName().str(), Range.getRange()));
  }

  G.allocActions().push_back(
      {{},
       cantFail(WrapperFunctionCall::Create<SPSCOFFDeregisterObjectSectionsArgs>(
           CP.orc_rt_coff_deregister_object_sections, HeaderAddr, ObjSecs))});

  auto& BState = CP.JDBootstrapStates[&JD];
  BState.ObjectSectionsMaps.push_back(std::move(ObjSecs));

  for(auto& S : G.sections())
    if(isCOFFInitializerSection(S.getName()))
      for(auto* B : S.blocks())
      {
        if(B->edges_empty())
          continue;
        for(auto& E : B->edges())
          BState.Initializers.push_back(std::make_pair(
              S.getName().str(), E.getTarget().getAddress() + E.getAddend()));
      }

  return Error::success();
}

}

#endif // defined(_WIN32) && !defined(_MSC_VER) && LLVM_VERSION_MAJOR >= 22
