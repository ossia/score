#pragma once

// "Does not crash on malformed input" is only a verdict if the crash cannot
// take the rest of the executable with it. These helpers run a callable in a
// forked child and report whether it came back cleanly, so one aborting parser
// shows up as a single red assertion instead of truncating the suite.

#include <catch2/catch_test_macros.hpp>

#include <QString>
#include <QtGlobal>

#if defined(__unix__) || defined(__APPLE__)
#define THREEDIM_HAS_FORK 1

#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstring>

namespace threedim_test
{
// True iff the child ran `f` and exited 0: no signal, no assert, and no
// ASan/UBSan diagnostic (those exit non-zero).
template <typename F>
bool survives(F&& f)
{
  std::fflush(nullptr);
  const pid_t pid = ::fork();
  if(pid == 0)
  {
    // Catch2's fatal-signal handler would print a full test report from the
    // child. Let the child just die with the signal instead: the parent reads
    // the verdict off waitpid().
    for(int sig : {SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE})
      std::signal(sig, SIG_DFL);
    // Qt's default message handler is not fork-safe on macOS: it reaches
    // Foundation, and Foundation kills any child that was forked without a
    // following exec() -- "Process N was forked to M without calling exec().
    // This is not supported by FileManager. Aborting." The abort is delivered
    // through os_log, so on a plain terminal the child just dies silently with
    // SIGABRT and the probe reports a crash the code under test never had.
    //
    // Route the child's logging to fputs instead. It keeps the diagnostics --
    // which is the point of logging a rejected file -- while touching nothing
    // but stdio, and it stops probe children from interleaving their expected
    // rejection messages into the parent's test output on every platform.
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&,
                              const QString& msg) {
      std::fputs(qPrintable(QStringLiteral("[fork-probe] ") + msg), stderr);
      std::fputc('\n', stderr);
    });
    f();
    ::_exit(0);
  }
  REQUIRE(pid > 0);
  int status = 0;
  ::waitpid(pid, &status, 0);
  if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
    return true;
  // Say HOW it died. A bare `false` sends every reader back to a debugger to
  // learn what a signal number would have told them: a SIGSEGV on one platform
  // and a SIGABRT on another are different defects, and the difference between
  // "the parser faulted" and "an assert fired" decides where to look.
  if(WIFSIGNALED(status))
    UNSCOPED_INFO("child died with signal " << WTERMSIG(status) << " ("
                  << ::strsignal(WTERMSIG(status)) << ")");
  else if(WIFEXITED(status))
    UNSCOPED_INFO("child exited " << WEXITSTATUS(status));
  return false;
}
} // namespace threedim_test
#endif
