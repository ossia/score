#pragma once

// "Does not crash on malformed input" is only a verdict if the crash cannot
// take the rest of the executable with it. These helpers run a callable in a
// forked child and report whether it came back cleanly, so one aborting parser
// shows up as a single red assertion instead of truncating the suite.

#include <catch2/catch_test_macros.hpp>

#if defined(__unix__) || defined(__APPLE__)
#define THREEDIM_HAS_FORK 1

#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>

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
    f();
    ::_exit(0);
  }
  REQUIRE(pid > 0);
  int status = 0;
  ::waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
} // namespace threedim_test
#endif
