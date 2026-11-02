#pragma once
// What the sweeps over every registered process share: a crash guard that turns
// a fatal signal into a recorded failure, a watchdog for hangs, the factories
// that cannot be built headlessly at all, and the SWEEP_ONLY filter.
//
// SWEEP_NOGUARD disables the guard so a debugger stops on the real fault;
// SWEEP_ONLY=<uuid>,<uuid> restricts a sweep to those factories.

#include <QtGlobal>

#include <unistd.h>

#include <csetjmp>
#include <csignal>
#include <cstdlib>
#include <set>
#include <string>

// Windows: no sigsetjmp/siglongjmp, SIGBUS/SIGTRAP/SIGALRM or alarm().
#if defined(_WIN32)
using sigjmp_buf = std::jmp_buf;
#define sigsetjmp(buf, save) setjmp(buf)
#define siglongjmp(buf, val) longjmp(buf, val)
static inline unsigned int alarm(unsigned int) noexcept
{
  return 0;
}
#define SIGALRM (-1)
#endif

namespace sweep
{
// A per-factory checkpoint, and one for the whole run: some nodes leave global
// or threaded engine state that segfaults at document teardown, after every
// result has been collected.
inline sigjmp_buf crash_jmp;
inline sigjmp_buf teardown_jmp;
inline volatile sig_atomic_t in_guard = 0;
inline volatile sig_atomic_t teardown_guard = 0;
inline volatile sig_atomic_t last_signal = 0;

extern "C" inline void crash_handler(int sig)
{
  if(in_guard)
  {
    in_guard = 0;
    last_signal = sig;
    siglongjmp(crash_jmp, sig);
  }
  if(teardown_guard)
  {
    teardown_guard = 0;
    last_signal = sig;
    siglongjmp(teardown_jmp, sig);
  }
  // Not in a guarded region: a real crash
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

inline constexpr int guarded_signals[] = {
    SIGSEGV, SIGABRT, SIGFPE, SIGILL
#if !defined(_WIN32)
    , SIGBUS, SIGTRAP, SIGALRM
#endif
};

inline void install_crash_handlers()
{
  for(int s : guarded_signals)
    std::signal(s, crash_handler);
}

inline void restore_crash_handlers()
{
  for(int s : guarded_signals)
    std::signal(s, SIG_DFL);
}

//! A unit of work running longer than this is aborted and recorded as a hang.
inline constexpr unsigned watchdog_seconds = 20;

//! Factories that cannot be constructed headlessly without blocking or crashing
//! outside of the guard; all of them need an external plug-in.
inline const std::set<std::string> construction_denylist = {
    // LV2: the lilv world scans the system plug-in directories, forever
    "fd5243ba-70b5-4164-b44a-ecb0dcdc0494",
    // VST 2: an empty plug-in path dereferences a null effect
    "be8e6bd3-75f2-4102-8895-8a4eb4ea545a",
    // VST 3: a module scan on a worker thread segfaults out of the guard's reach
    "4cc30435-3237-4e94-a79e-1c2bd6b724a1",
    // CLAP: the host runtime traps with no plug-in present
    "10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f",
};

inline bool no_guard()
{
  return qEnvironmentVariableIsSet("SWEEP_NOGUARD");
}

//! The factories SWEEP_ONLY selects; empty means all of them.
inline std::set<std::string> only()
{
  std::set<std::string> res;
  if(const char* v = std::getenv("SWEEP_ONLY"))
  {
    std::string tok;
    for(const char* c = v;; ++c)
    {
      if(*c == ',' || *c == '\0')
      {
        if(!tok.empty())
          res.insert(tok);
        tok.clear();
        if(*c == '\0')
          break;
      }
      else
        tok.push_back(*c);
    }
  }
  return res;
}

//! Runs f under the crash guard and the watchdog. Returns false, with what
//! happened appended to `detail`, when f crashed or hung.
template <typename F>
bool guarded(F&& f, std::string& detail)
{
  if(no_guard())
  {
    f();
    return true;
  }
  in_guard = 1;
  last_signal = 0;
  if(sigsetjmp(crash_jmp, 1) == 0)
  {
    alarm(watchdog_seconds);
    f();
    alarm(0);
    in_guard = 0;
    return true;
  }
  alarm(0);
  in_guard = 0;
  if(last_signal == SIGALRM)
    detail += " [TIMEOUT: exceeded " + std::to_string(watchdog_seconds) + "s]";
  else
    detail += " [CRASHED: fatal signal " + std::to_string((int)last_signal) + "]";
  return false;
}

//! Runs the whole sweep, escaping a crash at document or application teardown
//! that happens after the results are collected. Returns false in that case.
template <typename F>
bool run_with_teardown_guard(F&& f)
{
  if(no_guard())
  {
    f();
    return true;
  }
  install_crash_handlers();
  bool ok = true;
  if(sigsetjmp(teardown_jmp, 1) == 0)
  {
    teardown_guard = 1;
    f();
    teardown_guard = 0;
  }
  else
  {
    ok = false;
  }
  restore_crash_handlers();
  return ok;
}
}
