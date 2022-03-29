//===-- cc1_main.cpp - Clang CC1 Compiler Frontend ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the entry point to the clang -cc1 functionality, which implements the
// core compiler functionality along with a number of additional tools for
// demonstration and testing purposes.
//
//===----------------------------------------------------------------------===//

#undef CALLBACK
#include "clang/Basic/Stack.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/CodeGen/ObjectFilePCHContainerOperations.h"
#include "clang/Config/config.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/FrontendTool/Utils.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/BuryPointer.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
//#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TimeProfiler.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

#include <iostream>
#include <sstream>

#ifdef CLANG_HAVE_RLIMITS
#include <sys/resource.h>
#endif

using namespace clang;
using namespace llvm::opt;

class QtDiagnosticConsumer final : public DiagnosticConsumer
{

  // DiagnosticConsumer interface
public:
  void finish() override { std::cerr << " == finish == \n"; }
  void HandleDiagnostic(
      DiagnosticsEngine::Level DiagLevel,
      const Diagnostic& Info) override
  {
    SmallVector<char, 1024> vec;
    Info.FormatDiagnostic(vec);
    std::cerr << " == diagnostic == " << vec.data() << "\n";
  }
};

//===----------------------------------------------------------------------===//
// Main driver
//===----------------------------------------------------------------------===//
#if LLVM_VERSION_MAJOR < 14
static void
LLVMErrorHandler(void* UserData, const std::string& Message, bool GenCrashDiag)
#else
static void
LLVMErrorHandler(void* UserData, const char* Message, bool GenCrashDiag)
#endif
{
  DiagnosticsEngine& Diags = *static_cast<DiagnosticsEngine*>(UserData);

  Diags.Report(diag::err_fe_error_backend) << Message;

  // Run the interrupt handlers to make sure any special cleanups get done, in
  // particular that we remove files registered with RemoveFileOnSignal.
  llvm::sys::RunInterruptHandlers();

  // We cannot recover from llvm errors.  When reporting a fatal error, exit
  // with status 70 to generate crash diagnostics.  For BSD systems this is
  // defined as an internal software error.  Otherwise, exit with status 1.
  llvm::sys::Process::Exit(GenCrashDiag ? 70 : 1);
}

#ifdef CLANG_HAVE_RLIMITS
#if defined(__linux__) && defined(__PIE__)
static size_t getCurrentStackAllocation()
{
  // If we can't compute the current stack usage, allow for 512K of command
  // line arguments and environment.
  size_t Usage = 512 * 1024;
  if (FILE* StatFile = fopen("/proc/self/stat", "r"))
  {
    // We assume that the stack extends from its current address to the end of
    // the environment space. In reality, there is another string literal (the
    // program name) after the environment, but this is close enough (we only
    // need to be within 100K or so).
    unsigned long StackPtr, EnvEnd;
    // Disable silly GCC -Wformat warning that complains about length
    // modifiers on ignored format specifiers. We want to retain these
    // for documentation purposes even though they have no effect.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif
    if (fscanf(
            StatFile,
            "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %*lu "
            "%*lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %*lu %*ld %*lu %*lu "
            "%*lu %*lu %lu %*lu %*lu %*lu %*lu %*lu %*llu %*lu %*lu %*d %*d "
            "%*u %*u %*llu %*lu %*ld %*lu %*lu %*lu %*lu %*lu %*lu %lu %*d",
            &StackPtr,
            &EnvEnd)
        == 2)
    {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
      Usage = StackPtr < EnvEnd ? EnvEnd - StackPtr : StackPtr - EnvEnd;
    }
    fclose(StatFile);
  }
  return Usage;
}

#include <alloca.h>

LLVM_ATTRIBUTE_NOINLINE
static void ensureStackAddressSpace()
{
  // Linux kernels prior to 4.1 will sometimes locate the heap of a PIE binary
  // relatively close to the stack (they are only guaranteed to be 128MiB
  // apart). This results in crashes if we happen to heap-allocate more than
  // 128MiB before we reach our stack high-water mark.
  //
  // To avoid these crashes, ensure that we have sufficient virtual memory
  // pages allocated before we start running.
  size_t Curr = getCurrentStackAllocation();
  const int kTargetStack = DesiredStackSize - 256 * 1024;
  if (Curr < kTargetStack)
  {
    volatile char* volatile Alloc
        = static_cast<volatile char*>(alloca(kTargetStack - Curr));
    Alloc[0] = 0;
    Alloc[kTargetStack - Curr - 1] = 0;
  }
}
#else
static void ensureStackAddressSpace() { }
#endif

/// Attempt to ensure that we have at least 8MiB of usable stack space.
static void ensureSufficientStack()
{
  struct rlimit rlim;
  if (getrlimit(RLIMIT_STACK, &rlim) != 0)
    return;

  // Increase the soft stack limit to our desired level, if necessary and
  // possible.
  if (rlim.rlim_cur != RLIM_INFINITY
      && rlim.rlim_cur < rlim_t(DesiredStackSize))
  {
    // Try to allocate sufficient stack.
    if (rlim.rlim_max == RLIM_INFINITY
        || rlim.rlim_max >= rlim_t(DesiredStackSize))
      rlim.rlim_cur = DesiredStackSize;
    else if (rlim.rlim_cur == rlim.rlim_max)
      return;
    else
      rlim.rlim_cur = rlim.rlim_max;

    if (setrlimit(RLIMIT_STACK, &rlim) != 0
        || rlim.rlim_cur != DesiredStackSize)
      return;
  }

  // We should now have a stack of size at least DesiredStackSize. Ensure
  // that we can actually use that much, if necessary.
  ensureStackAddressSpace();
}
#else
static void ensureSufficientStack() { }
#endif

auto printErrors(TextDiagnosticBuffer& buf, const SourceManager& mgr)
{
  std::stringstream ss;
  for (auto it = buf.err_begin(); it != buf.err_end(); ++it)
  {
    auto& loc = it->first;
    ss << loc.printToString(mgr) << ":\n" << it->second << "\n\n";
 }

  std::cerr << ss.str();
  return ss.str();
}
llvm::Error cc1_main(
    ArrayRef<const char*> Argv,
    const char* Argv0,
    void* MainAddr)
{
  ensureSufficientStack();

  // Initialize targets first, so that --version shows registered targets.
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
#ifdef LINK_POLLY_INTO_TOOLS
  llvm::PassRegistry& Registry = *llvm::PassRegistry::getPassRegistry();
  polly::initializePollyPasses(Registry);
#endif

  // Buffer diagnostics from argument parsing so that we can output them using a
  // well formed diagnostic object.
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticBuffer* DiagsBuffer = new TextDiagnosticBuffer;
  llvm::IntrusiveRefCntPtr<DiagnosticsEngine> Diags(new DiagnosticsEngine(DiagID, &*DiagOpts, DiagsBuffer));

#if LLVM_VERSION_MAJOR >= 13
  llvm::IntrusiveRefCntPtr<FileManager> Files(new FileManager(FileSystemOptions(), llvm::vfs::getRealFileSystem()));
  llvm::IntrusiveRefCntPtr<SourceManager> SrcMgr(new SourceManager(*Diags, *Files));
#endif

  // Create a Clang instance
  std::unique_ptr<CompilerInstance> Clang(new CompilerInstance());

  // Register the support for object-file-wrapped Clang modules.
  auto PCHOps = Clang->getPCHContainerOperations();
  PCHOps->registerWriter(std::make_unique<ObjectFilePCHContainerWriter>());
  PCHOps->registerReader(std::make_unique<ObjectFilePCHContainerReader>());


  bool Success = CompilerInvocation::CreateFromArgs(
      Clang->getInvocation(), Argv, *Diags);

  if (Clang->getFrontendOpts().TimeTrace)
  {
    llvm::timeTraceProfilerInitialize(
        Clang->getFrontendOpts().TimeTraceGranularity, Argv0);
  }

#if LLVM_VERSION_MAJOR >= 13
  if(!Clang->hasSourceManager())
  {
    Diags->setSourceManager(SrcMgr.get());

    Clang->setFileManager(Files.get());
    Clang->setSourceManager(SrcMgr.get());
  }
#endif
  // Infer the builtin include path if unspecified.
  if (Clang->getHeaderSearchOpts().UseBuiltinIncludes
      && Clang->getHeaderSearchOpts().ResourceDir.empty())
    Clang->getHeaderSearchOpts().ResourceDir
        = CompilerInvocation::GetResourcesPath(Argv0, MainAddr);

  Clang->setDiagnostics(Diags.get());
  if (!Clang->hasDiagnostics())
  {
    return llvm::make_error<llvm::StringError>("No diagnostics", std::error_code(1, std::system_category()));
  }

  // Set an error handler, so that any LLVM backend diagnostics go through our
  // error handler.
  llvm::install_fatal_error_handler(
      LLVMErrorHandler, static_cast<void*>(&Clang->getDiagnostics()));

  DiagsBuffer->FlushDiagnostics(Clang->getDiagnostics());
  if (!Success)
  {
    return llvm::make_error<llvm::StringError>(printErrors(*DiagsBuffer, Clang->getSourceManager()), std::error_code(1, std::system_category()));
  }

  // Execute the frontend actions.
  {
    llvm::TimeTraceScope TimeScope("ExecuteCompiler");
    Success = ExecuteCompilerInvocation(Clang.get());
  }

  // If any timers were active but haven't been destroyed yet, print their
  // results now.  This happens in -disable-free mode.
  llvm::TimerGroup::printAll(llvm::errs());
  llvm::TimerGroup::clearAll();

  auto res = Success
                 ? llvm::Error::success()
                 : llvm::make_error<llvm::StringError>(printErrors(*DiagsBuffer, Clang->getSourceManager()), std::error_code(1, std::system_category()));


  // Our error handler depends on the Diagnostics object, which we're
  // potentially about to delete. Uninstall the handler now so that any
  // later errors use the default handling behavior instead.
  llvm::remove_fatal_error_handler();

  // When running with -disable-free, don't do any destruction or shutdown.
  if (Clang->getFrontendOpts().DisableFree)
  {
    llvm::BuryPointer(std::move(Clang));
    return res;
  }
  return res;
}
