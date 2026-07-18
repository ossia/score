#pragma once

// A MinGW-flavoured COFF Orc platform.
//
// score JIT-links its add-ons (libc++/libunwind, x86_64-w64-windows-gnu) with a
// hand-built ObjectLinkingLayer (JITLink) and, when the SDK ships compiler-rt's
// orc_rt, drives the executor through an Orc Platform so we get native TLS, real
// static-init/atexit scheduling and -- crucially on COFF -- SEH/.pdata exception
// registration.
//
// LLVM's stock COFFPlatform is MSVC-coupled: its constructor unconditionally
// bootstraps the VC runtime (vcruntime/msvcprt/cmt, via COFFVCRuntimeBootstrapper)
// and installs MSVC-only CXX aliases (_CxxThrowException -> orc_rt). Those cannot
// drive a MinGW process (libc++/libunwind, __gxx_personality_seh0). None of that
// machinery is overridable (private ctor + private members), so we cannot subclass
// it; instead this is a faithful copy of COFFPlatform's ABI-neutral core with the
// VC-runtime bootstrap and the _CxxThrowException alias stripped:
//   - __ImageBase header symbol per JITDylib,
//   - __orc_rt_coff_* bootstrap handshake,
//   - per-object __orc_rt_coff_register_object_sections (native .tls, SEH
//     .pdata/.xdata, .CRT$X* initializers),
//   - the ABI-neutral atexit/_onexit -> __orc_rt_coff_*_per_jd aliases.
// CRT / C++ / EH symbols are resolved from the host process (score.exe is itself
// MinGW-linked) through the process-symbols JITDylib.

#if defined(_WIN32) && !defined(_MSC_VER)

#include <llvm/Config/llvm-config.h>

#if LLVM_VERSION_MAJOR >= 22

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h>

#include <atomic>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Jit
{

/// Mediates between MinGW-flavoured COFF initialization and ExecutionSession
/// state. A VC-runtime-free subset of llvm::orc::COFFPlatform.
class MinGWCOFFPlatform : public llvm::orc::Platform
{
public:
  using LoadDynamicLibrary
      = llvm::unique_function<llvm::Error(llvm::orc::JITDylib& JD, llvm::StringRef DLLFileName)>;

  /// Try to create a MinGWCOFFPlatform instance, adding the ORC runtime to the
  /// given JITDylib. Mirrors COFFPlatform::Create minus the VC-runtime args.
  static llvm::Expected<std::unique_ptr<MinGWCOFFPlatform>> Create(
      llvm::orc::ObjectLinkingLayer& ObjLinkingLayer, llvm::orc::JITDylib& PlatformJD,
      std::unique_ptr<llvm::MemoryBuffer> OrcRuntimeArchiveBuffer,
      LoadDynamicLibrary LoadDynLibrary,
      std::optional<llvm::orc::SymbolAliasMap> RuntimeAliases = std::nullopt);

  static llvm::Expected<std::unique_ptr<MinGWCOFFPlatform>> Create(
      llvm::orc::ObjectLinkingLayer& ObjLinkingLayer, llvm::orc::JITDylib& PlatformJD,
      const char* OrcRuntimePath, LoadDynamicLibrary LoadDynLibrary,
      std::optional<llvm::orc::SymbolAliasMap> RuntimeAliases = std::nullopt);

  llvm::orc::ExecutionSession& getExecutionSession() const { return ES; }
  llvm::orc::ObjectLinkingLayer& getObjectLinkingLayer() const { return ObjLinkingLayer; }

  llvm::Error setupJITDylib(llvm::orc::JITDylib& JD) override;
  llvm::Error teardownJITDylib(llvm::orc::JITDylib& JD) override;
  llvm::Error
  notifyAdding(llvm::orc::ResourceTracker& RT, const llvm::orc::MaterializationUnit& MU) override;
  llvm::Error notifyRemoving(llvm::orc::ResourceTracker& RT) override;

  /// Run JD's collected static initializers (.CRT$XI*/.CRT$XC*) directly via
  /// runAsVoidFunction, exactly like the bootstrap does. orc_rt's COFF dlopen
  /// init path is MSVC-coupled (_initterm) and segfaults on MinGW, so the custom
  /// MinGWCOFFPlatformSupport calls this instead of going through orc_rt.
  llvm::Error initializeJITDylib(llvm::orc::JITDylib& JD);

  /// Default aliases for the platform (orc_rt utility forwarders).
  static llvm::orc::SymbolAliasMap standardPlatformAliases(llvm::orc::ExecutionSession& ES);

  /// CXX aliases. Unlike COFFPlatform we DROP _CxxThrowException (MSVC EH);
  /// MinGW exceptions go through libunwind / __gxx_personality_seh0 resolved from
  /// the host. Only the ABI-neutral orc_rt atexit/_onexit hooks remain.
  static llvm::ArrayRef<std::pair<const char*, const char*>> requiredCXXAliases();

  static llvm::ArrayRef<std::pair<const char*, const char*>> standardRuntimeUtilityAliases();

  static llvm::StringRef getSEHFrameSectionName() { return ".pdata"; }

private:
  using COFFJITDylibDepInfo = std::vector<llvm::orc::ExecutorAddr>;
  using COFFJITDylibDepInfoMap
      = std::vector<std::pair<llvm::orc::ExecutorAddr, COFFJITDylibDepInfo>>;
  using COFFObjectSectionsMap
      = llvm::SmallVector<std::pair<std::string, llvm::orc::ExecutorAddrRange>>;
  using PushInitializersSendResultFn
      = llvm::unique_function<void(llvm::Expected<COFFJITDylibDepInfoMap>)>;
  using SendSymbolAddressFn
      = llvm::unique_function<void(llvm::Expected<llvm::orc::ExecutorAddr>)>;
  using JITDylibDepMap
      = llvm::DenseMap<llvm::orc::JITDylib*, llvm::SmallVector<llvm::orc::JITDylib*>>;

  // Scans/modifies LinkGraphs to support COFF platform features (initializers,
  // exceptions, language-runtime registration).
  class MinGWCOFFPlatformPlugin : public llvm::orc::ObjectLinkingLayer::Plugin
  {
  public:
    MinGWCOFFPlatformPlugin(MinGWCOFFPlatform& CP)
        : CP(CP)
    {
    }

    void modifyPassConfig(
        llvm::orc::MaterializationResponsibility& MR, llvm::jitlink::LinkGraph& G,
        llvm::jitlink::PassConfiguration& Config) override;

    llvm::Error notifyFailed(llvm::orc::MaterializationResponsibility& MR) override
    {
      return llvm::Error::success();
    }

    llvm::Error notifyRemovingResources(llvm::orc::JITDylib& JD, llvm::orc::ResourceKey K) override
    {
      return llvm::Error::success();
    }

    void notifyTransferringResources(
        llvm::orc::JITDylib& JD, llvm::orc::ResourceKey DstKey,
        llvm::orc::ResourceKey SrcKey) override
    {
    }

  private:
    llvm::Error associateJITDylibHeaderSymbol(
        llvm::jitlink::LinkGraph& G, llvm::orc::MaterializationResponsibility& MR,
        bool Bootstrap);

    llvm::Error preserveInitializerSections(
        llvm::jitlink::LinkGraph& G, llvm::orc::MaterializationResponsibility& MR);
    llvm::Error
    registerObjectPlatformSections(llvm::jitlink::LinkGraph& G, llvm::orc::JITDylib& JD);
    llvm::Error registerObjectPlatformSectionsInBootstrap(
        llvm::jitlink::LinkGraph& G, llvm::orc::JITDylib& JD);

    std::mutex PluginMutex;
    MinGWCOFFPlatform& CP;
  };

  struct JDBootstrapState
  {
    llvm::orc::JITDylib* JD = nullptr;
    std::string JDName;
    llvm::orc::ExecutorAddr HeaderAddr;
    std::list<COFFObjectSectionsMap> ObjectSectionsMaps;
    llvm::SmallVector<std::pair<std::string, llvm::orc::ExecutorAddr>> Initializers;
  };

  static bool supportedTarget(const llvm::Triple& TT);

  MinGWCOFFPlatform(
      llvm::orc::ObjectLinkingLayer& ObjLinkingLayer, llvm::orc::JITDylib& PlatformJD,
      std::unique_ptr<llvm::orc::StaticLibraryDefinitionGenerator> OrcRuntimeGenerator,
      std::set<std::string> DylibsToPreload,
      std::unique_ptr<llvm::MemoryBuffer> OrcRuntimeArchiveBuffer,
      std::unique_ptr<llvm::object::Archive> OrcRuntimeArchive,
      LoadDynamicLibrary LoadDynLibrary, llvm::Error& Err);

  llvm::Error associateRuntimeSupportFunctions(llvm::orc::JITDylib& PlatformJD);
  llvm::Error bootstrapCOFFRuntime(llvm::orc::JITDylib& PlatformJD);
  llvm::Error runSymbolIfExists(llvm::orc::JITDylib& PlatformJD, llvm::StringRef SymbolName);

  llvm::Error runBootstrapInitializers(JDBootstrapState& BState);
  llvm::Error
  runBootstrapSubsectionInitializers(JDBootstrapState& BState, llvm::StringRef Start, llvm::StringRef End);

  llvm::Expected<JITDylibDepMap> buildJDDepMap(llvm::orc::JITDylib& JD);

  llvm::Expected<llvm::MemoryBufferRef> getPerJDObjectFile();

  void pushInitializersLoop(
      PushInitializersSendResultFn SendResult, llvm::orc::JITDylibSP JD, JITDylibDepMap& JDDepMap);

  void rt_pushInitializers(
      PushInitializersSendResultFn SendResult, llvm::orc::ExecutorAddr JDHeaderAddr);

  void rt_lookupSymbol(
      SendSymbolAddressFn SendResult, llvm::orc::ExecutorAddr Handle, llvm::StringRef SymbolName);

  // Run one JD's collected .CRT$XI* then .CRT$XC* initializers directly.
  llvm::Error runJDInitializers(llvm::orc::JITDylib& JD);

  llvm::orc::ExecutionSession& ES;
  llvm::orc::ObjectLinkingLayer& ObjLinkingLayer;

  LoadDynamicLibrary LoadDynLibrary;
  std::unique_ptr<llvm::MemoryBuffer> OrcRuntimeArchiveBuffer;
  std::unique_ptr<llvm::object::Archive> OrcRuntimeArchive;

  llvm::orc::SymbolStringPtr COFFHeaderStartSymbol;

  std::map<llvm::orc::JITDylib*, JDBootstrapState> JDBootstrapStates;
  std::atomic<bool> Bootstrapping;

  llvm::orc::ExecutorAddr orc_rt_coff_platform_bootstrap;
  llvm::orc::ExecutorAddr orc_rt_coff_platform_shutdown;
  llvm::orc::ExecutorAddr orc_rt_coff_register_object_sections;
  llvm::orc::ExecutorAddr orc_rt_coff_deregister_object_sections;
  llvm::orc::ExecutorAddr orc_rt_coff_register_jitdylib;
  llvm::orc::ExecutorAddr orc_rt_coff_deregister_jitdylib;

  llvm::DenseMap<llvm::orc::JITDylib*, llvm::orc::ExecutorAddr> JITDylibToHeaderAddr;
  llvm::DenseMap<llvm::orc::ExecutorAddr, llvm::orc::JITDylib*> HeaderAddrToJITDylib;

  llvm::DenseMap<llvm::orc::JITDylib*, llvm::orc::SymbolLookupSet> RegisteredInitSymbols;

  // Per-JD static initializers (.CRT$XI*/.CRT$XC* ctor addresses) collected at
  // materialization, run directly by initializeJITDylib() (see header above).
  llvm::DenseMap<
      llvm::orc::JITDylib*,
      llvm::SmallVector<std::pair<std::string, llvm::orc::ExecutorAddr>>>
      JDInitializers;
  // JDs whose initializers have already been run (bootstrap JD + initialized
  // add-ons), so we never run them twice.
  std::set<llvm::orc::JITDylib*> InitializedJDs;

  std::mutex PlatformMutex;
};

/// Serves the *weak-external aliases* of a mingw-w64 import library (the POSIX
/// old names: fileno -> _fileno, and dllimport variants: __imp_fileno ->
/// __imp__fileno) as ORC symbol aliases of their alternative name.
///
/// A driver link gets these from small alias objects inside libucrt.a, but
/// JITLink's COFF backend rejects them ("Weak external symbol with external
/// symbol as alternative not supported"), so a StaticLibraryDefinitionGenerator
/// over the import lib cannot load them. This generator parses the weak-external
/// records out of those objects itself -- data-driven from the toolchain's own
/// import lib, so the alias set always matches what the driver would produce --
/// and the aliased name then resolves through the JD's normal machinery (near
/// DLLImport stub to the UCRT export), same as an AOT link's alias thunk.
class COFFImportLibWeakAliasGenerator : public llvm::orc::DefinitionGenerator
{
public:
  static llvm::Expected<std::unique_ptr<COFFImportLibWeakAliasGenerator>>
  Load(const char* ImportLibPath);

  llvm::Error tryToGenerate(
      llvm::orc::LookupState& LS, llvm::orc::LookupKind K, llvm::orc::JITDylib& JD,
      llvm::orc::JITDylibLookupFlags JDLookupFlags,
      const llvm::orc::SymbolLookupSet& Symbols) override;

private:
  explicit COFFImportLibWeakAliasGenerator(llvm::StringMap<std::string> WeakToAlt)
      : WeakToAlt(std::move(WeakToAlt))
  {
  }

  llvm::StringMap<std::string> WeakToAlt; // weak name -> alternative name
};

/// LLJIT PlatformSupport that runs add-on static initializers via the platform's
/// direct path instead of orc_rt's MSVC-coupled COFF dlopen init (which segfaults
/// on MinGW). LLJIT::initialize(JD) -> this -> MinGWCOFFPlatform::initializeJITDylib.
class MinGWCOFFPlatformSupport : public llvm::orc::LLJIT::PlatformSupport
{
public:
  explicit MinGWCOFFPlatformSupport(MinGWCOFFPlatform& P)
      : P{P}
  {
  }
  llvm::Error initialize(llvm::orc::JITDylib& JD) override
  {
    return P.initializeJITDylib(JD);
  }
  llvm::Error deinitialize(llvm::orc::JITDylib& JD) override
  {
    return llvm::Error::success();
  }

private:
  MinGWCOFFPlatform& P;
};

}

#endif // LLVM_VERSION_MAJOR >= 22
#endif // defined(_WIN32) && !defined(_MSC_VER)
