// =============================================================================
// P2-14 (SPEC-SCENE-RENDER-TESTS.md §3.3, line 1125): closing a document
// mid-render tears down cleanly, and GfxContext::destroyOutput unregisters the
// output from the render clocks BEFORE it destroys it.
//
// REGISTRATION (exact line; this file is NOT registered by this commit --
// tests/gfx/CMakeLists.txt was out of scope, see "STATUS" below):
//
//     score_add_gfx_test(document_close_teardown GfxDocumentCloseTeardown.cpp)
//
// -> ctest name `test_gfx_document_close_teardown`. The directory is
// tests/gfx/: this drives the real plug-in's GfxContext through
// Gfx::DocumentPlugin, which is exactly what tests/gfx/GfxEdgeConsumeLatch.cpp
// does, and it needs GFX_TEST_CORPUS_DIR for its ISF producer -- both of which
// score_add_gfx_test supplies (tests/gfx/CMakeLists.txt:161-169).
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_document_close_teardown
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_document_close_teardown
//
// -----------------------------------------------------------------------------
// SPEC CORRECTION. §3.3 P2-14 cites `GfxContext.cpp:106-148` for
// destroyOutput's unregister-before-destroy order. Those line numbers are
// stale in this tree: at
// src/plugins/score-plugin-gfx/Gfx/GfxContext.cpp, lines 106-148 straddle
// register_preview_node / unregister_node and only the FIRST half of
// destroyOutput. The function is
//   GfxContext::destroyOutput            GfxContext.cpp:124-175
//   the render-clock unregister loop     GfxContext.cpp:138-145
//   m_graph->destroyOutputRenderList()   GfxContext.cpp:147
// and the rationale comment ("leaving a node that is about to be freed in that
// list is a use-after-free as soon as the next queued tick fires") is
// GfxContext.cpp:132-137. All line numbers below were read in this tree.
//
// -----------------------------------------------------------------------------
// WHAT IS ACTUALLY OBSERVED, AND HOW.
//
// The rig is the real one: score::test::new_document(), the document's
// Gfx::DocumentPlugin (GfxApplicationPlugin.hpp:12-20, installed on every
// created document by ApplicationPlugin::on_createdDocument,
// GfxApplicationPlugin.cpp:27-30), its GfxContext, and the real producer
// channel GfxExecutionAction::setEdge/endTick (GfxExecContext.hpp:28-29) --
// the same rig GfxEdgeConsumeLatch.cpp:107-170 uses. One ISF producer
// (corpus/isf-solid-color.fs) feeds one offscreen BackgroundNode sink, which
// is what the app's headless window device owns
// (Gfx/Window/OffscreenDevice.hpp:29,39-43).
//
// The sink is a ProbeOutput: a BackgroundNode subclass that counts, in file-
// scope globals (so they survive the node being freed), every call the RENDER
// CLOCK makes on it. That is the whole trick. GfxContext::recomputeTimers
// gives every output with a manualRenderingRate to a score::gfx::TimerClock
// (GfxContext.cpp:395-426) whose tick closure is, verbatim
// (GfxContext.cpp:417-421):
//
//     clock->start([clock] {
//       for(auto* out : clock->outputs())
//         if(out && out->canRender())
//           out->render();
//     });
//
// So `canRender()` and `render()` are called on the output ONCE PER TICK, by
// the clock, for exactly as long as the output is in TimerClock::m_outputs
// (RenderClock.hpp:94-97). Counting them is a direct runtime observation of
// clock membership -- not a re-reading of the source:
//
//   * BEFORE the teardown, both counters must be non-zero. This is the
//     negative control for the whole file: without it every assertion below
//     would pass on a graph that never rendered anything at all.
//     (Measured, identical on OpenGL and Vulkan: 5 renders / 13 canRender
//     queries before the teardown; in the destroyOutput case destroy_seq=37
//     against last_render_seq=34, i.e. the last clock callback strictly
//     precedes the destroy, and 50 witness ticks / 0 victim callbacks in the
//     400 ms afterwards.)
//   * AFTER the teardown, both must be exactly zero across ~400 ms of pumped
//     event loop (>= 24 ticks at the 60 Hz the test pins). A single query
//     means the clock still holds the raw OutputNode* -- which is the
//     use-after-free GfxContext.cpp:132-137 describes, because in production
//     the node is freed moments later (~offscreen_device,
//     OffscreenDevice.hpp:80-97; or ~GfxContext's `nodes` map).
//
// THE WITNESS. "Zero callbacks afterwards" is only evidence if callbacks were
// still POSSIBLE. GfxContext::destroyOutput also ERASES a TimerClock that has
// become empty (GfxContext.cpp:141-142), so a lone output leaves no clock at
// all and the silence would prove nothing. The destroyOutput case therefore
// registers TWO sinks at the same rate -- which recomputeTimers coalesces onto
// ONE shared TimerClock (GfxContext.cpp:402-424) -- and tears only the first
// one down. The second, the witness, must go on being ticked throughout the
// post-teardown pump; if it does not, the child reports CHILD_CLOCK_STOPPED
// and the case fails as inconclusive rather than passing on a dead loop. With
// the negative control below applied, the victim stays in that same,
// demonstrably-live clock, so its counters cannot stay at zero.
//
// The probe additionally stamps a monotonic sequence on every render() and on
// destroyOutput(), so "render after destroy" is reported as an ordering fact
// (last_render_seq vs destroy_seq) and not only as a count.
//
// OBSERVATION LIMIT, stated plainly. This observes that the unregister
// HAPPENS, and that no clock callback reaches the output after the teardown
// began. It does NOT distinguish a pure statement swap -- moving the loop
// (GfxContext.cpp:138-145) to just after destroyOutputRenderList
// (GfxContext.cpp:147) with nothing in between -- because nothing pumps the
// event loop between those two statements, so no tick can be delivered in the
// gap and there is nothing to see. Making the probe's destroyOutput() pump the
// loop itself WAS tried and rejected: at that moment
// Graph::releaseOutputRenderList has already done `output.setRenderer({})`
// (Graph.cpp:1206-1228) while renderState() is still live, so
// GfxContext::updateGraph -> Graph::createMissingRenderLists
// (Graph.cpp:341-345) would REBUILD the render list in the middle of the
// teardown -- a self-inflicted crash on a correct build, i.e. a fake red.
// What is asserted instead is the strongest thing observable from outside the
// plug-in: the unregister happened, before control ever leaves destroyOutput.
//
// -----------------------------------------------------------------------------
// NEGATIVE CONTROL (spec: "destroy before unregistering from the render clock
// -> UAF"). The hook is exact:
//
//   FILE: src/plugins/score-plugin-gfx/Gfx/GfxContext.cpp
//   EDIT: delete lines 138-145, i.e. the whole loop
//
//           for(auto it = m_renderClocks.begin(); it != m_renderClocks.end();)
//           {
//             (*it)->removeOutput(node);
//             if((*it)->empty())
//               it = m_renderClocks.erase(it);
//             else
//               ++it;
//           }
//
//         leaving GfxContext::destroyOutput to go straight to
//         m_graph->destroyOutputRenderList(*node) at line 147. That is the
//         pre-fix state the comment at GfxContext.cpp:132-137 names ("The
//         async REMOVE_NODE path already does this; this synchronous one must
//         too" -- the async twin is GfxContext.cpp:709-717).
//
//   MUST REDDEN: case 2, "the render clock lets go of an output before
//   destroyOutput destroys it", with child verdict
//   CHILD_CLOCK_STILL_HOLDS_OUTPUT (47) and a message naming the non-zero
//   canRender/render counts of the VICTIM sink. (The witness sink keeps the
//   shared TimerClock alive and ticking, so the victim is guaranteed to be
//   visited.) Case 1 (document close) is NOT expected to redden from this
//   edit alone: the document-close path goes through ~GfxContext, which stops
//   the clocks by a different route (m_renderClocks.clear() at
//   GfxContext.cpp:76, before `delete m_graph` at GfxContext.cpp:85).
//
//   The ~GfxContext half has its own control: swap GfxContext.cpp:76
//   (`m_renderClocks.clear();`) to AFTER GfxContext.cpp:85 (`delete m_graph;`)
//   -- ~Graph then calls out->destroyOutput() (Graph.cpp:1129-1132) on nodes
//   the clocks still point at, and the timer pool is still alive because
//   std::destroy_at(&m_timers) (GfxContext.cpp:82) has not run either. That
//   reddens case 1 as a crash or a non-zero post-destroy count.
//
// -----------------------------------------------------------------------------
// WHY FORK-ISOLATED. The failure mode under test is a use-after-free during
// teardown: unfixed it is a SIGSEGV / SIGABRT / an ASan abort, and CTest's
// WILL_FAIL does not invert abnormal termination. Running the scenario in a
// forked child turns "the product died" into ONE red assertion in the parent
// ("child killed by signal N") instead of a dead ctest run. Same shape,
// verbatim, as tests/gfx/GfxIncrementalResizeFork.cpp:105-271.
//
// PARENT STAYS GRAPHICS-FREE BEFORE FORKING, for the same reason that file
// gives (GfxIncrementalResizeFork.cpp:20-37): forking a process that already
// owns a GL/Vulkan context or a booted QApplication is undefined behaviour --
// driver threads, fds and locks do not survive fork(). The parent here only
// calls platform_backends() (a qgetenv; see score_test/Gfx.hpp:168-192),
// fork()s, waitpid()s and drains a status pipe. The CHILD boots its own GUI
// app via run_in_gui_app, creates the document, the QRhi and the graph, and
// _exit()s a numeric verdict.
//
// SKIPS (cleanly): no fork (non-unix, THREEDIM_HAS_FORK unset by
// score_test/ForkProbe.hpp:10-11); no usable RHI backend for the generated
// api (probe_api, score_test/Gfx.hpp:197); no document delegate. A backend
// that cannot come up is CHILD_SKIP, never a silent Null-backend pass -- the
// house rule in SPEC §3.0 ("Hardware").
//
// STATUS / HONESTY NOTE. The registration line above is present at
// tests/gfx/CMakeLists.txt:1036; it was not added by the change that wrote
// this file. (It has to be there: cmake/ScoreTestRegistrationGuard.cmake
// FATAL_ERRORs the configure when tests/ holds a .cpp that no ctest entry
// reaches.)
//
// What was executed while writing it (not through ctest -- the object was
// compiled and linked by hand against the b-dyn build's own flags and link
// line for test_gfx_incremental_resize_fork, which is why the registration
// above is the one that matches):
//   * OpenGL and Vulkan, 3 consecutive runs each: all green,
//     12 assertions / 2 test cases.
//   * headless (DISPLAY / WAYLAND_DISPLAY / QT_QPA_PLATFORM all unset):
//     2 cases skipped, cleanly.
//   * a SIMULATED negative control -- the victim stamped as destroyed WITHOUT
//     GfxContext::destroyOutput being called, i.e. the observable effect of
//     deleting GfxContext.cpp:138-145 -- reddens case 2 exactly as designed:
//     exit 47, "canRender=50 render=25 after destroy (destroy_seq=37,
//     last_render_seq=185); witness ticks in the same window=50", while case 1
//     stays green. The real source edit was NOT applied (src/ was out of
//     scope), so that is a simulation of its effect, not a run of it.
// =============================================================================

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/ForkProbe.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Settings/Model.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>

namespace
{
QString corpus(const char* name)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(name);
}
} // namespace

#if defined(THREEDIM_HAS_FORK)

#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstring>

namespace
{
// ---------------------------------------------------------------------------
// Child verdict codes. Distinct from 0/1 so an unrelated exit(1) (a Qt fatal,
// an ASan report) is reported as what it is instead of masquerading as one of
// ours -- same convention as GfxIncrementalResizeFork.cpp:84-92.
// ---------------------------------------------------------------------------
enum ChildVerdict : int
{
  CHILD_OK = 0,
  CHILD_SKIP = 42,                    // no backend / no document: parent SKIPs
  CHILD_BUILD_FAILED = 43,            // the ISF producer would not compile
  CHILD_NO_RENDERLIST = 44,           // the sink never got a RenderList
  CHILD_NO_RENDER = 45,               // the render clock never ticked the sink
  CHILD_NOT_TORN_DOWN = 46,            // destroyOutput() never ran on the sink
  CHILD_CLOCK_STILL_HOLDS_OUTPUT = 47, // THE defect: clock callbacks after destroy
  CHILD_CLOCK_STOPPED = 48 // inconclusive: the witness stopped being ticked too
};

// ---------------------------------------------------------------------------
// The observation. File-scope because the probe node is freed by the product
// (by ~GfxContext's `nodes` map on the document-close path), and the verdict
// has to be readable AFTER that -- reading it off the object would be the very
// use-after-free under test.
//
// Single-threaded by construction: every writer below runs on the UI thread
// (the TimerClock marshals its tick onto its owner's thread,
// RenderClock.hpp:80-84,110-118), and each forked child runs exactly one
// scenario.
// ---------------------------------------------------------------------------
long long g_seq = 0;

/// Per-sink tally. Slot 0 is the VICTIM (the sink that gets torn down), slot 1
/// the WITNESS (kept alive to prove the shared clock is still ticking).
struct Observations
{
  int renders = 0;            // render() calls, from the clock tick closure
  int can_render_queries = 0; // canRender() calls, likewise
  int renders_after_destroy = 0;
  int can_render_after_destroy = 0;
  int destroy_calls = 0;
  long long last_render_seq = 0;
  long long last_can_render_seq = 0;
  long long destroy_seq = 0;
};

constexpr int VICTIM = 0;
constexpr int WITNESS = 1;
Observations g_obs[2];

void reset_observations()
{
  g_seq = 0;
  g_obs[VICTIM] = {};
  g_obs[WITNESS] = {};
}

/// The offscreen sink the app's headless window device owns
/// (OffscreenDevice.hpp:29,39-43), instrumented at the two points the render
/// clock's tick closure touches (GfxContext.cpp:417-421) and at the teardown
/// point Graph::destroyOutputRenderList calls (Graph.cpp:1230-1235).
///
/// Nothing is faked: the base BackgroundNode still creates its real QRhi in
/// createOutput() and still renders real frames in render().
struct ProbeOutput final : score::gfx::BackgroundNode
{
  explicit ProbeOutput(int slot)
      : m_slot{slot}
  {
  }

  bool canRender() const override
  {
    auto& o = g_obs[m_slot];
    ++o.can_render_queries;
    o.last_can_render_seq = ++g_seq;
    if(o.destroy_seq != 0)
      ++o.can_render_after_destroy;
    return score::gfx::BackgroundNode::canRender();
  }

  void render() override
  {
    auto& o = g_obs[m_slot];
    ++o.renders;
    o.last_render_seq = ++g_seq;
    if(o.destroy_seq != 0)
      ++o.renders_after_destroy;
    score::gfx::BackgroundNode::render();
  }

  void destroyOutput() override
  {
    // Stamp BEFORE delegating: from here on the output is being torn down, so
    // any clock callback that still arrives is a callback on a dying node.
    // (~BackgroundNode also calls destroyOutput(), but from a destructor, so
    // that call dispatches to the base version and is not counted here --
    // BackgroundNode.hpp:29.)
    auto& o = g_obs[m_slot];
    ++o.destroy_calls;
    if(o.destroy_seq == 0)
      o.destroy_seq = ++g_seq;
    score::gfx::BackgroundNode::destroyOutput();
  }

private:
  int m_slot{};
};

// ---------------------------------------------------------------------------
// Event-loop pumps. Local rather than WindowedOutputCommon.hpp's, which drags
// in ScreenNode / MultiWindowNode and a real platform surface this file does
// not want.
// ---------------------------------------------------------------------------
void pump_for(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

template <typename Pred>
bool pump_until(Pred&& pred, int ms)
{
  QElapsedTimer t;
  t.start();
  while(!pred())
  {
    if(t.elapsed() > ms)
      return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return true;
}

/// True when a GUI application can plausibly be constructed here. Pure
/// environment inspection: no Qt object, no graphics, so it is safe in the
/// PARENT, before the fork. run_in_gui_app does NOT force the offscreen
/// platform (score_test/App.hpp:140), so on a display-less box the child would
/// die on QApplication construction and be reported as a signal death -- which
/// would read as the crash this file is hunting. SKIP instead.
bool has_display()
{
  if(qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    return true;
#if defined(__APPLE__) || defined(_WIN32)
  return true;
#else
  return qEnvironmentVariableIsSet("DISPLAY")
         || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
#endif
}

QString api_setting_name(score::gfx::GraphicsApi api)
{
  const Gfx::Settings::GraphicsApis apis{};
  switch(api)
  {
    case score::gfx::Vulkan:
      return apis.Vulkan;
    case score::gfx::Metal:
      return apis.Metal;
    case score::gfx::D3D11:
      return apis.D3D11;
    case score::gfx::D3D12:
      return apis.D3D12;
    default:
      return apis.OpenGL;
  }
}

/// Which teardown route the child drives.
enum class Teardown
{
  //! ctx.docManager.forceCloseDocument() while the clock is ticking. Runs
  //! ~Gfx::DocumentPlugin -> ~GfxContext (GfxContext.cpp:64-86).
  CloseDocument,
  //! GfxContext::destroyOutput() (GfxContext.cpp:124-175), the synchronous
  //! path a device-owned output takes (OffscreenDevice.hpp:88-90).
  DestroyOutput
};

struct ChildRun
{
  int verdict = CHILD_BUILD_FAILED;
  std::string message;
};

/// Everything below runs in the FORKED CHILD. No Catch2 macros: the child
/// encodes its verdict in the exit code and writes one human-readable line on
/// the status pipe (GfxIncrementalResizeFork.cpp:39-44).
ChildRun run_child_scenario(score::gfx::GraphicsApi backend, Teardown how)
{
  ChildRun out;
  reset_observations();

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    // Pin the clock the assertions count on: no vsync (so recomputeTimers
    // takes the TimerClock branch, GfxContext.cpp:351-427) and 60 Hz, which
    // is also what BackgroundNode's ctor turns into its manualRenderingRate
    // (BackgroundNode.hpp:22-26).
    auto& settings = ctx.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    settings.setRate(60.);
    settings.setGraphicsApi(api_setting_name(backend));

    // Never fall back to a Null/offscreen device for a case whose subject is a
    // live render clock: SKIP instead (SPEC §3.0, "Hardware").
    std::string backendName;
    if(!score::test::gfx::probe_api(backend, backendName))
    {
      out.verdict = CHILD_SKIP;
      out.message = std::string("RHI backend '")
                    + score::test::gfx::backend_name(backend)
                    + "' unavailable on this machine";
      return;
    }

    score::Document* doc = score::test::new_document(ctx);
    if(!doc)
    {
      out.verdict = CHILD_SKIP;
      out.message = "no document delegate";
      return;
    }

    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    // A real producer: without one, BackgroundNode::render() short-circuits
    // (renderers.size() > 1 gate, BackgroundNode.hpp:37-51) and nothing is
    // ever recorded on the GPU -- "mid-render" would be a lie.
    auto built = score::test::gfx::make_isf_node(corpus("isf-solid-color.fs"));
    if(!built.node)
    {
      out.verdict = CHILD_BUILD_FAILED;
      out.message = built.error;
      return;
    }

    // The victim, and -- only for the destroyOutput case -- a witness sink at
    // the same rate, so the two coalesce onto one shared TimerClock
    // (GfxContext.cpp:402-424) and the clock is NOT erased when the victim
    // leaves it (GfxContext.cpp:141-142). Without it, "no callbacks after the
    // teardown" would be satisfied by a clock that no longer exists.
    const bool wantWitness = (how == Teardown::DestroyOutput);

    auto victimOwned = std::make_unique<ProbeOutput>(VICTIM);
    victimOwned->shared_readback = std::make_shared<QRhiReadbackResult>();
    ProbeOutput* victim = victimOwned.get();

    std::unique_ptr<ProbeOutput> witnessOwned;
    if(wantWitness)
    {
      witnessOwned = std::make_unique<ProbeOutput>(WITNESS);
      witnessOwned->shared_readback = std::make_shared<QRhiReadbackResult>();
    }

    const int32_t a = g.register_node(std::move(built.node));
    const int32_t s0 = g.register_node(std::move(victimOwned));
    const int32_t s1 = wantWitness ? g.register_node(std::move(witnessOwned)) : -1;

    // The real producer channel, exactly as GfxEdgeConsumeLatch.cpp:128-140
    // drives it: every tick re-sends the complete edge set; endTick publishes
    // it when it differs from the previous tick's.
    {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      exec.setEdge(
          Gfx::port_index{a, 0}, Gfx::port_index{s0, 0},
          Process::CableType::ImmediateGlutton);
      if(wantWitness)
        exec.setEdge(
            Gfx::port_index{a, 0}, Gfx::port_index{s1, 0},
            Process::CableType::ImmediateGlutton);
      exec.endTick(st);
    }

    // One synchronous turn lands the nodes and the edges; a sink is an
    // OutputNode, so its ADD_NODE forces recompute_graph -> recomputeTimers,
    // which is what creates the TimerClock (GfxContext.cpp:751-759, 395-426).
    g.updateGraph();

    if(!victim->renderer() || !victim->renderState())
    {
      out.verdict = CHILD_NO_RENDERLIST;
      out.message = std::string(score::test::gfx::backend_name(backend))
                    + ": the sink never got a RenderList (renderer="
                    + (victim->renderer() ? "set" : "null") + ", renderState="
                    + (victim->renderState() ? "set" : "null") + ")";
      return;
    }

    // Let the REAL render clock drive it. Nothing here calls render()
    // by hand: every count below comes from the TimerClock tick closure.
    pump_until([] { return g_obs[VICTIM].renders >= 5; }, 5000);

    const Observations before = g_obs[VICTIM];
    if(before.renders == 0 || before.can_render_queries == 0)
    {
      out.verdict = CHILD_NO_RENDER;
      out.message = std::string(score::test::gfx::backend_name(backend))
                    + ": the render clock never reached the output (renders="
                    + std::to_string(before.renders) + ", canRender queries="
                    + std::to_string(before.can_render_queries) + ")";
      return;
    }
    const int witnessBefore = g_obs[WITNESS].can_render_queries;

    // ---------------------------------------------------------------------
    // THE EVENT.
    // ---------------------------------------------------------------------
    switch(how)
    {
      case Teardown::CloseDocument:
        // Closing while the clock is running. From here the sink is owned by
        // the product and about to be freed: only the globals may be read.
        victim = nullptr;
        ctx.docManager.forceCloseDocument(ctx, *doc);
        doc = nullptr;
        pump_for(400);
        break;

      case Teardown::DestroyOutput:
        // The synchronous device-owned-output path
        // (OffscreenDevice.hpp:88-90). It RELEASES the GfxContext's ownership
        // of the node (GfxContext.cpp:169-173), so the sink is ours to free
        // afterwards -- which is also what keeps the post-teardown pump below
        // a legal read rather than a use-after-free of its own.
        g.destroyOutput(victim);
        // >= 24 ticks at 60 Hz. Every canRender()/render() the victim sees in
        // this window is the clock still holding a pointer it was told to drop.
        pump_for(400);
        break;
    }

    const Observations v = g_obs[VICTIM];
    const int witnessTicks = g_obs[WITNESS].can_render_queries - witnessBefore;

    if(v.destroy_calls == 0)
    {
      out.verdict = CHILD_NOT_TORN_DOWN;
      out.message = std::string(score::test::gfx::backend_name(backend))
                    + ": destroyOutput() never ran on the output, so nothing "
                      "was actually torn down";
    }
    else if(wantWitness && witnessTicks == 0)
    {
      // Inconclusive, not green: the shared clock stopped delivering ticks at
      // all, so the victim's zero counts prove nothing.
      out.verdict = CHILD_CLOCK_STOPPED;
      out.message = std::string(score::test::gfx::backend_name(backend))
                    + ": the witness sink was not ticked once during the "
                      "post-teardown pump, so the silence on the destroyed "
                      "output is not evidence";
    }
    else if(v.renders_after_destroy != 0 || v.can_render_after_destroy != 0)
    {
      out.verdict = CHILD_CLOCK_STILL_HOLDS_OUTPUT;
      out.message
          = std::string(score::test::gfx::backend_name(backend))
            + ": the render clock kept calling a destroyed output -- canRender="
            + std::to_string(v.can_render_after_destroy)
            + " render=" + std::to_string(v.renders_after_destroy)
            + " after destroy (destroy_seq=" + std::to_string(v.destroy_seq)
            + ", last_render_seq=" + std::to_string(v.last_render_seq)
            + ", last_canRender_seq=" + std::to_string(v.last_can_render_seq)
            + "); witness ticks in the same window=" + std::to_string(witnessTicks);
    }
    else
    {
      out.verdict = CHILD_OK;
      out.message = std::string(score::test::gfx::backend_name(backend))
                    + ": ok (renders before teardown=" + std::to_string(before.renders)
                    + ", canRender queries="
                    + std::to_string(before.can_render_queries)
                    + ", destroy_seq=" + std::to_string(v.destroy_seq)
                    + ", last_render_seq=" + std::to_string(v.last_render_seq)
                    + ", witness ticks after teardown=" + std::to_string(witnessTicks)
                    + ")";
    }

    if(victim)
    {
      // GfxContext::destroyOutput released its unique_ptr
      // (GfxContext.cpp:169-173); we are the owner now, exactly as
      // ~offscreen_device's m_node is (OffscreenDevice.hpp:29,95-96).
      delete victim;
      victim = nullptr;
    }
  });

  return out;
}

/// What the parent learned from one forked run. Shape copied from
/// GfxIncrementalResizeFork.cpp:94-103.
struct ForkedRun
{
  bool fork_failed = false;
  bool exited = false;
  int exit_code = -1;
  bool signaled = false;
  int term_signal = 0;
  std::string message;
};

ForkedRun fork_and_run(score::gfx::GraphicsApi backend, Teardown how)
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
    // Restore default fatal-signal dispositions: Catch2's handlers would
    // print a bogus report from the child, and the parent reads the verdict
    // off waitpid() -- the same convention ForkProbe.hpp:30-34 documents.
    for(int sig : {SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE})
      std::signal(sig, SIG_DFL);
    ::close(fds[0]);

    const ChildRun r = run_child_scenario(backend, how);

    if(!r.message.empty())
    {
      const auto n = ::write(fds[1], r.message.data(), r.message.size());
      (void)n; // best-effort: the exit code is the verdict, the text is gravy
    }
    ::close(fds[1]);
    ::_exit(r.verdict);
    // ----------------------------- END CHILD -----------------------------
  }

  // Parent: reap first (messages are far below PIPE_BUF, so the child never
  // blocks on the pipe), then drain the status text.
  ::close(fds[1]);
  int status = 0;
  ::waitpid(pid, &status, 0);

  char buf[1024];
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

/// The parent half both cases share: a signal death is the crash this file
/// exists to turn into an assertion.
void judge(const ForkedRun& run, score::gfx::GraphicsApi backend, const char* what)
{
  REQUIRE_FALSE(run.fork_failed);

  INFO(
      "child status: exited=" << run.exited << " code=" << run.exit_code
                              << " signaled=" << run.signaled
                              << " sig=" << run.term_signal
                              << " message=" << run.message);

  if(run.signaled)
    FAIL(
        "child killed by signal "
        << run.term_signal << " (" << strsignal(run.term_signal) << ") while " << what
        << " on " << score::test::gfx::backend_name(backend)
        << " -- P2-14: the teardown is not clean");

  REQUIRE(run.exited);

  if(run.exit_code == CHILD_SKIP)
    SKIP(run.message);

  // Any other non-zero code is one of the child's own checks; the message says
  // which (build / no render list / no render / not torn down / clock still
  // holding the output).
  REQUIRE(run.exit_code == CHILD_OK);
}
} // namespace

TEST_CASE(
    "closing a document mid-render tears down cleanly",
    "[gfx][l3][document][lifecycle][fork]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  if(!has_display())
    SKIP("no windowing system: this rig boots the real GUI stack");

  const ForkedRun run = fork_and_run(backend, Teardown::CloseDocument);
  judge(run, backend, "closing the document with the render clock running");
}

TEST_CASE(
    "the render clock lets go of an output before destroyOutput destroys it",
    "[gfx][l3][document][lifecycle][fork]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  if(!has_display())
    SKIP("no windowing system: this rig boots the real GUI stack");

  const ForkedRun run = fork_and_run(backend, Teardown::DestroyOutput);
  judge(run, backend, "tearing an output down through GfxContext::destroyOutput");
}

#else // !THREEDIM_HAS_FORK

TEST_CASE(
    "closing a document mid-render tears down cleanly",
    "[gfx][l3][document][lifecycle][fork]")
{
  SKIP("fork-based child isolation is a unix-only harness (ForkProbe.hpp:10); "
       "a teardown crash here would kill the whole binary rather than produce "
       "a verdict");
}

TEST_CASE(
    "the render clock lets go of an output before destroyOutput destroys it",
    "[gfx][l3][document][lifecycle][fork]")
{
  SKIP("fork-based child isolation is a unix-only harness (ForkProbe.hpp:10)");
}

#endif
