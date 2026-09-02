// =============================================================================
// P0-5 -- an incremental add-new-output must not leak a render pass.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_render_pass_leak
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_render_pass_leak
//
// WHY THIS TEST EXISTS. The historical Vulkan render-pass leak (one VkRenderPass
// left behind by every incremental add-new-output) is reported FIXED by the
// pass-creation/reconcile rework -- but the existing coverage,
// "FINDING add-new-output incremental" in tests/gfx/GfxIncrementalFindings.cpp,
// only asserts that the ADDED OUTPUT'S PIXELS APPEAR. It counts nothing: a
// build that re-leaks a QRhiRenderPassDescriptor (and the render target it
// belongs to) on every add/remove cycle still renders magenta and stays green
// there. This file makes the leak COUNTABLE, three ways, so a regression trips:
//
//  1. PER-CYCLE (Vulkan/D3D12): QRhi::statistics().allocCount -- the memory
//     allocator's live-allocation count (VMA on Vulkan; see QRhiStats in
//     qtbase/src/gui/rhi/qrhi.h:1896). A leaked render target keeps its color
//     texture allocation alive, so over K add-output/remove-output cycles the
//     count must return to its steady-state baseline after EVERY cycle (+-0,
//     cycles 1..K-1; cycle 0 is excluded as cache warm-up). OpenGL/Metal
//     expose no allocator statistics (allocCount stays 0) and rely on 2+3.
//
//  2. AT TEARDOWN (every backend, unix): Qt's own RHI resource accounting.
//     With QT_RHI_LEAK_CHECK=1 (always-on in a debug Qt), destroying a QRhi
//     that still has registered resources prints, per resource:
//         <Type> resource 0x... (<debug name>)
//     after "QRhi %p going down with %d unreleased resources ..."
//     (qtbase/src/gui/rhi/qrhi.cpp:8722-8729). Every render pass the engine
//     creates is a named resource ("createRenderTarget::renderPass",
//     src/plugins/score-plugin-gfx/Gfx/Graph/Utils.cpp:161) and properly
//     released resources NEVER appear there: TextureRenderTarget::release()
//     goes through QRhiResource::deleteLater() and QRhi::~QRhi flushes the
//     pending-delete list BEFORE the leak check runs (qrhi.cpp:9200). So the
//     count of "RenderPassDescriptor resource" / "TextureRenderTarget
//     resource" lines in the teardown stderr is exactly the number of leaked
//     passes / render targets, and must be 0. stderr is captured around the
//     pipeline teardown with the dup2 pattern from GfxEdgeConsumeLatch.cpp.
//
//  3. VULKAN VALIDATION (gated on the layer's presence at runtime): score
//     enables VK_LAYER_KHRONOS_validation when SCORE_GPU_VALIDATION is set
//     (src/lib/score/gfx/Vulkan.cpp:35-44); the layer's object-lifetime
//     tracking reports every VkRenderPass "has not been destroyed" at
//     vkDestroyDevice, which happens inside the captured teardown. Zero such
//     lines required WHEN the layer is available; silently informational when
//     it is not (a machine without the layer must not fail here).
//
// THE SCENARIO is the FINDING's graph ops, extended into a loop of K = 20
// cycles: producer `a` -> sink s0 stays up the whole time; sink s1 is created
// up front but left unwired (an OutputNode only receives a RenderList from
// createAllRenderLists -- a sink CANNOT be added after create(), which is why
// the FINDING pre-creates s1 too); each cycle then adds a fresh producer `b`,
// wires it to s1 through the app's real incremental path (addEdge + reconcile
// + createAllMissingPasses + updateAllSinkSamplers), renders, checks the
// output APPEARS (the cycle must be real), and tears it back down through the
// real removal path (onEdgeRemoved + removeEdge + reconcile;
// removeNodeAndEdges). The renderer resources of the now-unreachable `b` are
// released by reconcileAllRenderLists (Graph.cpp:945, releaseState) -- the
// exact site whose omission the counters above must catch, along with
// releaseOutputRenderList's renderer->release() (Graph.cpp:1218) at teardown.
//
// RSS is deliberately NOT used: it cannot attribute a leak to a render pass.
//
// MEASURED LIMITATION (verified by the orchestrator's negative controls):
// a PURE QRhiRenderPassDescriptor leak (renderPass->deleteLater() removed
// from TextureRenderTarget::release()) trips mechanism 2 on Vulkan
// ("leaked RenderPassDescriptors=2") but is INVISIBLE on OpenGL: Qt's
// teardown report only lists resources that "own native graphics
// resources", and a QGles2RenderPassDescriptor owns none. On OpenGL this
// test still catches leaks of textures / render targets / whole render
// lists (verified: skipping ~Graph's renderer->release() goes red on GL
// through the same counter), but the render-pass-specific strong form is
// Vulkan/D3D12-only. Run the Vulkan leg to guard the render pass itself.
// =============================================================================
#include "GfxIncrementalCommon.hpp"

#include <score/gfx/Vulkan.hpp>
#if QT_HAS_VULKAN
#include <QVulkanInstance>
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#define GFX_LEAK_HAVE_STDERR_CAPTURE 1
#else
#define GFX_LEAK_HAVE_STDERR_CAPTURE 0
#endif

#include <algorithm>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
constexpr int K = 20; // add-output/remove-output cycles

// Both env gates must be decided before the FIRST QRhi of the process is
// destroyed / the static QVulkanInstance is created, i.e. before any test
// body runs. Set-if-unset so an explicit caller setting (e.g.
// SCORE_GPU_VALIDATION=2 soak, or =0 to silence a broken local layer) wins.
const bool g_env_init = [] {
  const auto setDefault = [](const char* name, const char* value) {
    if(!qEnvironmentVariableIsSet(name))
      qputenv(name, value);
  };
  // Qt release builds only run the QRhiImplementation dtor leak report when
  // this is set (qrhi.cpp:8718); debug Qt builds always do.
  setDefault("QT_RHI_LEAK_CHECK", "1");
  // Ask score for VK_LAYER_KHRONOS_validation (Vulkan.cpp:35); harmless when
  // the layer does not exist (score just warns it is unavailable).
  setDefault("SCORE_GPU_VALIDATION", "1");
  // Keep Qt logging on fd 2 even under service managers.
  setDefault("QT_FORCE_STDERR_LOGGING", "1");
  return true;
}();

/// Everything is collected inside the app lambda and asserted after
/// run_in_gui_app returns (Catch2 macros throw; see the fixture header).
struct LeakShot
{
  bool skipped = false;
  std::string skip_reason, backend, error;

  int cycles_run = 0;
  std::vector<bool> appeared;            // per-cycle: the added output rendered
  quint64 allocAfterCreate = 0;          // sum of QRhiStats::allocCount, both sinks
  std::vector<quint64> allocAfterCycle;  // sampled after each cycle's removal+flush
  bool statsUsable = false;              // backend exposes allocator statistics

  bool vkValidationPresent = false;      // VK_LAYER_KHRONOS_validation available
  std::string teardownLog;               // stderr captured around pipeline teardown
};

#if GFX_LEAK_HAVE_STDERR_CAPTURE
/// Captures everything written to fd 2 (both fprintf(stderr, ...) and
/// qWarning through any installed handler -- SafeQApplication::DebugOutput
/// also writes to stderr on non-MSVC) between construction and finish().
/// Same shape as tests/gfx/GfxEdgeConsumeLatch.cpp, plus a best-effort pipe
/// enlargement so a torrent of validation output cannot block the writer.
struct StderrCapture
{
  int saved{-1};
  int fds[2]{-1, -1};

  StderrCapture()
  {
    ::fflush(stderr);
    saved = ::dup(2);
    ::pipe(fds);
#if defined(F_SETPIPE_SZ)
    ::fcntl(fds[1], F_SETPIPE_SZ, 1 << 20);
#endif
    ::dup2(fds[1], 2);
    ::close(fds[1]);
  }

  std::string finish()
  {
    ::fflush(stderr);
    ::dup2(saved, 2);
    ::close(saved);
    std::string r;
    char buf[4096];
    ssize_t n;
    while((n = ::read(fds[0], buf, sizeof buf)) > 0)
      r.append(buf, std::size_t(n));
    ::close(fds[0]);
    return r;
  }
};
#endif

/// Number of non-overlapping occurrences of `needle` in `hay`.
int countOccurrences(const std::string& hay, const char* needle)
{
  const std::size_t len = std::char_traits<char>::length(needle);
  int n = 0;
  for(std::size_t pos = hay.find(needle); pos != std::string::npos;
      pos = hay.find(needle, pos + len))
    ++n;
  return n;
}

/// Sum of the "going down with N unreleased resources" totals in `log`
/// (one line per destroyed QRhi that still had registered resources).
int qtLeakTotal(const std::string& log)
{
  static const std::regex re{"going down with (\\d+) unreleased resources"};
  int total = 0;
  for(auto it = std::sregex_iterator(log.begin(), log.end(), re);
      it != std::sregex_iterator(); ++it)
    total += std::stoi((*it)[1].str());
  return total;
}

/// Lines where the Khronos validation layer reports a VkRenderPass leaked at
/// device teardown ("... VkRenderPass 0x... has not been destroyed").
int vkRenderPassLeaks(const std::string& log)
{
  int n = 0;
  std::size_t start = 0;
  while(start < log.size())
  {
    std::size_t end = log.find('\n', start);
    if(end == std::string::npos)
      end = log.size();
    const std::string_view line{log.data() + start, end - start};
    if(line.find("VkRenderPass") != std::string_view::npos
       && line.find("has not been destroyed") != std::string_view::npos)
      ++n;
    start = end + 1;
  }
  return n;
}
} // namespace

TEST_CASE(
    "P0-5 incremental add-output/remove-output cycles do not leak render "
    "passes or render targets",
    "[gfx][l3][incremental][leak]")
{
  (void)g_env_init;
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  LeakShot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    // unique_ptr so the FULL teardown (graph -> render lists -> QRhis) can be
    // forced inside the stderr capture, before the app itself goes down.
    auto p = std::make_unique<GfxPipeline>();

    // The FINDING scenario's graph ops: producer `a` keeps s0 alive across
    // the whole run; s1 pre-created (see the file-top comment on why) and
    // activated per cycle by a fresh producer.
    const int a = p->addIsf(corpus("isf-solid-color.fs"));
    const int s0 = p->addSink({64, 64});
    const int s1 = p->addSink({64, 64});
    p->wire(p->imageOut(a, 0), p->sinkInput(s0));

    if(!p->create(backend))
    {
      out.skipped = p->skipped();
      out.skip_reason = p->skipReason();
      out.backend = p->backend();
      out.error = p->error();
      return;
    }
    out.backend = p->backend();

#if QT_HAS_VULKAN
    if(backend == score::gfx::Vulkan)
      if(auto* inst = score::gfx::staticVulkanInstance(/*create=*/false))
        out.vkValidationPresent = inst->supportedLayers().contains(
            QByteArrayLiteral("VK_LAYER_KHRONOS_validation"));
#endif

    // Live-allocation count over both sinks' QRhis (each sink owns its QRhi;
    // the per-cycle producer renders inside s1's RenderList / QRhi). Non-zero
    // only on backends whose QRhi runs a memory allocator (Vulkan, D3D12).
    const auto sumAlloc = [&]() -> quint64 {
      quint64 n = 0;
      for(int i : {s0, s1})
        if(auto rs = p->sink(i)->renderState(); rs && rs->rhi)
          n += rs->rhi->statistics().allocCount;
      return n;
    };

    p->render(3);
    out.allocAfterCreate = sumAlloc();
    out.statsUsable = out.allocAfterCreate > 0;

    for(int cycle = 0; cycle < K; ++cycle)
    {
      // ADD-OUTPUT: fresh producer -> the idle sink, through the app's real
      // incremental path (addEdge + reconcile + missing passes + samplers).
      const int b = p->addIsf(corpus("isf-solid-color.fs"));
      if(b < 0)
        break; // p->error() is set; reported below
      p->addEdgeIncremental(p->imageOut(b, 0), p->sinkInput(s1));
      p->render(3);

      // The cycle must be real: the added output has to actually appear,
      // exactly as the FINDING asserts. A leak counter on a dead pipeline
      // would count nothing.
      out.appeared.push_back(solid(p->readback(s1), {255, 0, 255, 255}, 2));

      // REMOVE-OUTPUT: the same incremental removal the app performs
      // (onEdgeRemoved + removeEdge + reconcile, then removeNodeAndEdges).
      p->removeEdgeIncremental(p->imageOut(b, 0), p->sinkInput(s1));
      p->removeNodeIncremental(b);

      // Flush: TextureRenderTarget::release() defers through
      // QRhiResource::deleteLater() (flushed at end*Frame) and the Vulkan
      // backend further defers the VMA free by up to FRAMES_IN_FLIGHT (2)
      // frames -- 3 frames settle both before the count is sampled.
      p->render(3);
      out.allocAfterCycle.push_back(sumAlloc());
      ++out.cycles_run;
    }
    out.error = p->error();

    // COUNT 2+3: tear the whole pipeline down (render lists released, then
    // each sink's QRhi destroyed) under stderr capture. Qt's leak check and
    // the validation layer's object tracker both report into this window.
#if GFX_LEAK_HAVE_STDERR_CAPTURE
    {
      StderrCapture cap;
      p.reset();
      out.teardownLog = cap.finish();
    }
#else
    p.reset();
#endif
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.cycles_run == K);

  // The cycles were real: the added output appeared every time.
  {
    int failedCycles = 0;
    for(const bool ok : out.appeared)
      if(!ok)
        ++failedCycles;
    INFO("cycles whose added output did not render: " << failedCycles);
    CHECK(failedCycles == 0);
  }

  // COUNT 1 -- per-cycle allocator accounting (Vulkan/D3D12). Steady state
  // from cycle 1 on: the live-allocation count must return to the same value
  // after every add/remove cycle (+-0). One leaked render target per cycle
  // makes this climb by >= 1 per cycle (~+18 over the compared range).
  if(out.statsUsable)
  {
    REQUIRE(out.allocAfterCycle.size() == std::size_t(K));
    const auto first = out.allocAfterCycle.begin() + 1;
    const auto [mn, mx] = std::minmax_element(first, out.allocAfterCycle.end());
    INFO(
        "allocCount after create=" << out.allocAfterCreate << " cycle1=" << *first
                                   << " min=" << *mn << " max=" << *mx
                                   << " (cycles 1.." << K - 1 << ")");
    CHECK(*mn == *mx);
  }
  else
  {
    INFO(
        "backend '" << out.backend
                    << "' exposes no allocator statistics (QRhiStats::allocCount "
                       "== 0); per-cycle accounting not available, relying on "
                       "the teardown resource accounting below");
  }

#if GFX_LEAK_HAVE_STDERR_CAPTURE
  // COUNT 2 -- Qt RHI resource accounting at QRhi teardown. A leaked pass or
  // render target is, by construction, still registered on the QRhi when it
  // is destroyed, and prints one attributable line each.
  {
    const int leakedPasses
        = countOccurrences(out.teardownLog, "RenderPassDescriptor resource");
    const int leakedTargets
        = countOccurrences(out.teardownLog, "TextureRenderTarget resource");
    const int namedEnginePasses
        = countOccurrences(out.teardownLog, "createRenderTarget::renderPass");
    const int total = qtLeakTotal(out.teardownLog);
    INFO(
        "QRhi teardown accounting: leaked RenderPassDescriptors=" << leakedPasses
            << " leaked TextureRenderTargets=" << leakedTargets
            << " (of which engine-created passes=" << namedEnginePasses
            << "), total unreleased resources reported by Qt=" << total);
    INFO("teardown stderr:\n" << out.teardownLog);
    CHECK(leakedPasses == 0);
    CHECK(leakedTargets == 0);
  }

  // COUNT 3 -- Vulkan validation layer object tracking at vkDestroyDevice,
  // asserted only when the layer actually exists on this machine.
  {
    const int vkLeaks = vkRenderPassLeaks(out.teardownLog);
    if(out.vkValidationPresent)
    {
      INFO("VK_LAYER_KHRONOS_validation present; leaked VkRenderPass lines=" << vkLeaks);
      CHECK(vkLeaks == 0);
    }
    else if(vkLeaks > 0)
    {
      // Not gated in: the layer still spoke, so believe it.
      INFO("validation layer not detected as present, but it reported "
           << vkLeaks << " leaked VkRenderPass(es) at teardown");
      CHECK(vkLeaks == 0);
    }
    else
    {
      INFO("VK_LAYER_KHRONOS_validation not present (or non-Vulkan backend); "
           "validation-layer half of P0-5 not applicable on this run");
    }
  }
#else
  INFO("no stderr capture on this platform; only the per-cycle allocator "
       "accounting ran");
#endif
}
