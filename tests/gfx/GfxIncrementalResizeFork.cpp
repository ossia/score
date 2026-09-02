// =============================================================================
// P0-4 (SPEC-SCENE-RENDER-TESTS.md): resize after an incremental add survives
// on every backend — FORK-ISOLATED regression guard.
//
// HISTORY. This exact scenario (render, resize the sink mid-render twice, read
// back) used to SIGSEGV on Vulkan: the incremental path left a stale
// VkRenderPass behind and the resize-triggered pipeline rebuild dereferenced
// it. That defect was captured as the FINDING titled "resize after an
// incremental add crashes on Vulkan" in GfxIncrementalFindings.cpp; per
// LEDGER-DEFECT-FIXES.md the fix landed and both GfxIncrementalFindings pins
// are green on OpenGL and Vulkan. This file is NOT a defect pin (no
// [!shouldfail] — the correct behaviour is the current behaviour): it re-runs
// the identical graph operations inside a forked child, so that a FUTURE
// regression of the fix shows up as a recorded verdict ("child killed by
// SIGSEGV") instead of a dead ctest run that takes the whole binary with it.
// (CTest's WILL_FAIL does not invert abnormal terminations, so only a child
// process turns a re-introduced crash into an assertable value — same
// rationale as GfxDropEmptyNodelistAbort.cpp.)
//
// FORK + GPU SAFETY ARGUMENT. Forking a process that already owns a GL/Vulkan
// context (or a booted QApplication) is undefined-behaviour territory: driver
// threads, fds and locks do not survive fork(). This test therefore forks
// FIRST, before any Qt GUI boot or QRhi creation in this process:
//   * the PARENT never calls run_in_gui_app, never constructs a GfxPipeline,
//     never touches QRhi — it only fork()s, waitpid()s and reads a status
//     pipe. The only pre-fork work is platform_backends() (a qgetenv, no app
//     needed — see its comment in score_test/Gfx.hpp) and Catch2 bookkeeping.
//   * the CHILD does ALL graphics work: it boots its own GUI app via
//     run_in_gui_app (MinimalGUIApplication is constructed per call, inside
//     the child), creates the QRhi, renders, resizes, reads back — exactly the
//     single-process test does — then _exit()s with a verdict code.
// This differs from the existing fork users: PrimitiveMeshes.cpp /
// GfxDropEmptyNodelistAbort.cpp fork CPU-only work (the latter even forks
// after app boot, which its header justifies by the child touching no event
// loop / no GPU). We are the first GPU fork user, hence fork-before-any-
// graphics: at fork time this process is single-threaded (Catch2 runner) with
// no Qt application object, so the child is a clean slate.
//
// CHILD -> PARENT PROTOCOL. The child must not run Catch2 macros (fork +
// Catch2 do not mix; ForkProbe.hpp resets the fatal-signal handlers for the
// same reason). The child performs the Catch2-free readback checks itself
// (GfxIncrementalCommon.hpp's solid() is a plain inline function) and encodes
// the verdict in its exit code; a one-line human-readable message travels over
// a pipe for SKIP/INFO text. The parent's REQUIREs interpret the verdict.
//
//   exit 0                  scenario ran; final readback is 40x40 solid
//                           magenta {255,0,255,255} (tol 2) — the same
//                           expectation GfxIncrementalFindings.cpp:84-86 pins.
//   exit CHILD_SKIP (42)    backend unavailable on this box -> parent SKIPs.
//   exit CHILD_* (43..46)   pipeline error / invalid / wrong-size / wrong-
//                           colour readback -> parent fails with the message.
//   killed by a signal      THE regression this file guards: the parent fails
//                           with "child killed by signal N".
//
// Runs on every backend platform_backends() yields (GENERATE), with
// per-backend SKIP. On non-unix (no THREEDIM_HAS_FORK) the whole case SKIPs:
// fork-based isolation is a unix-only harness, same degradation as
// GfxDropEmptyNodelistAbort.cpp.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_incremental_resize_fork
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_incremental_resize_fork
// =============================================================================
#include "GfxIncrementalCommon.hpp"

#include <score_test/ForkProbe.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

#if defined(THREEDIM_HAS_FORK)

#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstring>
#include <string>

namespace
{
// Child verdict codes. Distinct from 0/1 so an unrelated exit(1) (Qt fatal,
// ASan) is reported as what it is instead of masquerading as a verdict.
enum ChildVerdict : int
{
  CHILD_OK = 0,
  CHILD_SKIP = 42,          // backend unavailable: parent turns this into SKIP
  CHILD_PIPELINE_ERROR = 43,// create()/render error reported by the fixture
  CHILD_BAD_READBACK = 44,  // final readback invalid (no pixels)
  CHILD_BAD_SIZE = 45,      // final readback is not 40x40
  CHILD_BAD_COLOR = 46,     // final readback is not solid magenta
};

/// What the parent learned from one forked run.
struct ForkedRun
{
  bool fork_failed = false;
  bool exited = false;   // WIFEXITED
  int exit_code = -1;    // WEXITSTATUS if exited
  bool signaled = false; // WIFSIGNALED
  int term_signal = 0;   // WTERMSIG if signaled
  std::string message;   // whatever the child wrote on the status pipe
};

/// Fork; run the P0-4 scenario in the child on `backend`; reap the verdict.
/// The scenario is a faithful clone of GfxIncrementalFindings.cpp:55-77
/// ("resize after an incremental add"): one ISF solid-colour node wired to one
/// 64x64 sink, render 3, resize to 96x48 (the historical SIGSEGV point),
/// render 3, resize to 40x40, render 3, read back.
inline ForkedRun run_scenario_forked(score::gfx::GraphicsApi backend)
{
  ForkedRun out;

  int fds[2]{-1, -1};
  if(::pipe(fds) != 0)
  {
    out.fork_failed = true;
    out.message = "pipe() failed";
    return out;
  }

  std::fflush(nullptr); // don't let the child re-flush buffered parent output
  const pid_t pid = ::fork();
  if(pid < 0)
  {
    ::close(fds[0]);
    ::close(fds[1]);
    out.fork_failed = true;
    out.message = "fork() failed";
    return out;
  }

  if(pid == 0)
  {
    // ------------------------------- CHILD -------------------------------
    // No Catch2 macros from here on. Restore default fatal-signal dispositions
    // (Catch2's handlers would print a bogus report from the child; the parent
    // reads the verdict off waitpid) — same convention as ForkProbe.hpp.
    for(int sig : {SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE})
      std::signal(sig, SIG_DFL);
    ::close(fds[0]);

    int verdict = CHILD_PIPELINE_ERROR;
    std::string msg;

    // The child boots its OWN gui app; the parent never did. All QRhi /
    // backend work happens on this side of the fork.
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      // ---- graph ops cloned from GfxIncrementalFindings.cpp:56-76 ----
      GfxPipeline p;
      const int a = p.addIsf(corpus("isf-solid-color.fs"));
      const int s0 = p.addSink({64, 64});
      p.wire(p.imageOut(a, 0), p.sinkInput(s0));

      if(!p.create(backend))
      {
        if(p.skipped())
        {
          verdict = CHILD_SKIP;
          msg = p.backend() + ": " + p.skipReason();
        }
        else
        {
          verdict = CHILD_PIPELINE_ERROR;
          msg = p.backend() + ": " + p.error();
        }
        return;
      }

      p.render(3);
      p.resizeSink(s0, {96, 48}); // historically: SIGSEGV here on Vulkan
      p.render(3);
      p.resizeSink(s0, {40, 40});
      p.render(3);
      const auto c = p.readback(s0);
      // ---- end of cloned scenario ----

      // Catch2-free versions of the original CHECKs
      // (GfxIncrementalFindings.cpp:82-86), most specific failure first.
      if(!p.error().empty())
      {
        verdict = CHILD_PIPELINE_ERROR;
        msg = p.backend() + ": " + p.error();
      }
      else if(!c.valid())
      {
        verdict = CHILD_BAD_READBACK;
        msg = p.backend() + ": final readback invalid";
      }
      else if(c.width != 40 || c.height != 40)
      {
        verdict = CHILD_BAD_SIZE;
        msg = p.backend() + ": final readback is " + std::to_string(c.width)
              + "x" + std::to_string(c.height) + ", expected 40x40";
      }
      else if(!solid(c, {255, 0, 255, 255}, 2))
      {
        verdict = CHILD_BAD_COLOR;
        msg = p.backend() + ": final readback is not solid magenta";
      }
      else
      {
        verdict = CHILD_OK;
        msg = p.backend() + ": ok";
      }
    });

    if(!msg.empty())
    {
      const auto n = ::write(fds[1], msg.data(), msg.size());
      (void)n; // best-effort: the exit code is the verdict, the text is gravy
    }
    ::close(fds[1]);
    ::_exit(verdict);
    // ----------------------------- END CHILD -----------------------------
  }

  // Parent: reap first (the messages are far below PIPE_BUF, so the child
  // never blocks on the pipe), then drain the status text.
  ::close(fds[1]);
  int status = 0;
  ::waitpid(pid, &status, 0);

  char buf[512];
  ssize_t n;
  while((n = ::read(fds[0], buf, sizeof buf)) > 0)
    out.message.append(buf, std::size_t(n));
  ::close(fds[0]);

  out.exited = WIFEXITED(status);
  out.exit_code = out.exited ? WEXITSTATUS(status) : -1;
  out.signaled = WIFSIGNALED(status);
  out.term_signal = out.signaled ? WTERMSIG(status) : 0;
  return out;
}
} // namespace

TEST_CASE(
    "resize after an incremental add survives on every backend (fork-isolated)",
    "[gfx][l3][incremental][fork]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const ForkedRun run = run_scenario_forked(backend);
  REQUIRE_FALSE(run.fork_failed);

  INFO(
      "child status: exited=" << run.exited << " code=" << run.exit_code
                              << " signaled=" << run.signaled
                              << " sig=" << run.term_signal
                              << " message=" << run.message);

  // A signal death is exactly the regression this guard exists for.
  if(run.signaled)
    FAIL(
        "child killed by signal " << run.term_signal << " ("
                                  << strsignal(run.term_signal)
                                  << ") — the P0-4 resize-after-incremental-add "
                                     "crash is back on "
                                  << backend_name(backend));

  REQUIRE(run.exited);

  if(run.exit_code == CHILD_SKIP)
    SKIP(run.message);

  // Any other non-zero code is the child's own readback check failing; the
  // message says which one (pipeline error / invalid / size / colour).
  REQUIRE(run.exit_code == CHILD_OK);
}

#else // !THREEDIM_HAS_FORK

TEST_CASE(
    "resize after an incremental add survives on every backend (fork-isolated)",
    "[gfx][l3][incremental][fork]")
{
  SKIP("fork-based child isolation is a unix-only harness; the unforked "
       "scenario still runs in test_gfx_incremental_findings");
}

#endif
