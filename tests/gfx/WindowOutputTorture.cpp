// Torture the three windowed output modes -- single-window (ScreenNode),
// multi-window (MultiWindowNode) and background (BackgroundNode) -- with a
// randomised sequence of the disruptions a user can inflict from the UI, and
// assert that the output is still presenting frames afterwards.
//
// The point is recovery, not any single operation: hiding, re-showing, resizing,
// overriding the render size and going in and out of full screen each leave the
// window in a different combination of m_running / m_hasSwapChain / m_notExposed
// / m_closed, and the paths that clear those are not symmetric with the paths
// that set them. A sequence is what finds the combination a single case misses.
//
// Both clocks are covered, because they fail differently. With vsync the loop is
// a self-perpetuating requestUpdate() chain that Qt stops delivering to an
// unexposed window, so a disruption can strand it permanently; without vsync a
// wall timer keeps calling render() regardless, and the same state machine has
// to converge from the other side.
//
// Seeded: SCORE_TORTURE_SEED replays an exact sequence, SCORE_TORTURE_OPS sets
// how many operations each case performs.
//
//   DISPLAY=:0 ctest -R gfx_window_output_torture
//   SCORE_TORTURE_SEED=12345 SCORE_TORTURE_OPS=60 DISPLAY=:0 ctest -R gfx_window_output_torture

#include "WindowedOutputCommon.hpp"

#include <QCloseEvent>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <random>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
unsigned seed_from_env()
{
  bool ok = false;
  const unsigned s = qEnvironmentVariableIntValue("SCORE_TORTURE_SEED", &ok);
  return ok && s != 0 ? s : 20260823u;
}

int ops_from_env()
{
  bool ok = false;
  const int n = qEnvironmentVariableIntValue("SCORE_TORTURE_OPS", &ok);
  return ok && n > 0 ? n : 24;
}

struct Outcome
{
  bool skipped{};
  std::string skipReason;
  std::string error;
  std::string backend;
};

/// Which clock drives the output. They strand the state machine differently, so
/// every mode below is tortured under both.
struct Clock
{
  const char* name;
  score::gfx::OutputNode::Configuration conf;
};

std::vector<Clock> clocks()
{
  return {
      {"vsync", {.manualRenderingRate = {}, .supportsVSync = true}},
      {"timer", {.manualRenderingRate = 1000. / 60., .supportsVSync = false}}};
}

/// The disruptions a user can reach from the UI, closing the window included:
/// that path releases the swap chain from inside Window::event() and is the one
/// that used to strand the output entirely.
enum class Op
{
  Hide,
  Show,
  Close,
  Resize,
  RenderSize,
  ClearRenderSize,
  FullScreenOff,
  Raise,
  Count
};

const char* op_name(Op o)
{
  switch(o)
  {
    case Op::Hide:
      return "hide";
    case Op::Show:
      return "show";
    case Op::Close:
      return "close";
    case Op::Resize:
      return "resize";
    case Op::RenderSize:
      return "renderSize";
    case Op::ClearRenderSize:
      return "clearRenderSize";
    case Op::FullScreenOff:
      return "fullScreenOff";
    case Op::Raise:
      return "raise";
    default:
      return "?";
  }
}
}

TEST_CASE(
    "a single-window output recovers from any sequence of window disruptions",
    "[gfx][window][screen][torture]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto clock = GENERATE(from_range(clocks()));

  const unsigned seed = seed_from_env();
  const int nops = ops_from_env();

  Outcome o;
  std::string log;
  int presentedAfter{};
  bool exposedAfter{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(
           api, {256, 192}, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"),
           clock.conf))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();

    auto* win = rig.window();
    REQUIRE(win != nullptr);

    int presented = 0;
    auto inner = win->onRender;
    win->onRender = [&presented, inner](QRhiCommandBuffer& cb) {
      ++presented;
      if(inner)
        inner(cb);
    };

    // With vsync the window drives itself; without it the caller does.
    const bool vsync = bool(clock.conf.supportsVSync) && !clock.conf.manualRenderingRate;
    int ticks = 0;
    if(vsync)
      rig.screen->setVSyncCallback([&] { ++ticks; });

    auto settle = [&] {
      if(vsync)
        pump_for(120);
      else
        rig.render(4);
    };

    std::mt19937 rng{seed};
    std::uniform_int_distribution<int> pick{0, int(Op::Count) - 1};
    for(int i = 0; i < nops; ++i)
    {
      const auto op = Op(pick(rng));
      log += op_name(op);
      log += ' ';
      switch(op)
      {
        case Op::Hide:
          win->hide();
          pump_until([&] { return !win->isExposed(); }, 2000);
          break;
        case Op::Show:
          win->show();
          pump_until([&] { return win->isExposed(); }, 3000);
          break;
        case Op::Close:
        {
          QCloseEvent close;
          QCoreApplication::sendEvent(win, &close);
          pump_for(150);
          break;
        }
        case Op::Resize:
          win->resize(200 + (i * 17) % 180, 150 + (i * 13) % 120);
          break;
        case Op::RenderSize:
          rig.screen->setRenderSize(QSize{128 + (i * 7) % 96, 96 + (i * 5) % 72});
          break;
        case Op::ClearRenderSize:
          rig.screen->setRenderSize(QSize{});
          break;
        case Op::FullScreenOff:
          rig.screen->setFullScreen(false);
          break;
        case Op::Raise:
          win->raise();
          break;
        default:
          break;
      }
      settle();
    }

    // Whatever the sequence did, the window ends visible and must present again.
    win->show();
    pump_until([&] { return win->isExposed(); }, 5000);
    exposedAfter = win->isExposed();

    presented = 0;
    ticks = 0;
    if(vsync)
      pump_for(1500);
    else
      rig.render(30);
    presentedAfter = presented;

    if(vsync)
      rig.screen->setVSyncCallback({});
    win->onRender = inner;
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  INFO("backend=" << o.backend << " clock=" << clock.name << " seed=" << seed
                  << " (SCORE_TORTURE_SEED to replay)");
  INFO("sequence: " << log);
  REQUIRE(o.error.empty());
  CHECK(exposedAfter);
  CHECK(presentedAfter > 0);
}

TEST_CASE(
    "a multi-window output recovers from any sequence of window disruptions",
    "[gfx][window][multiwindow][torture]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto clock = GENERATE(from_range(clocks()));

  const unsigned seed = seed_from_env();
  const int nops = ops_from_env();

  Outcome o;
  std::string log;
  int fpsBefore{}, fpsAfter{};
  bool canRenderAfter{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    MultiWindowRig rig;
    std::vector<Gfx::OutputMapping> maps;
    maps.push_back({});
    maps.push_back({});
    if(!rig.build(api, maps, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs")))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();

    // MultiWindowNode routes no per-frame callback through Window::onRender, so
    // the observable here is the swap chains themselves: hiding releases them and
    // a recovered output has to have created them again. Weaker than counting
    // frames, and it is what this node exposes.
    rig.render(10);
    for(const auto& wo : rig.node->windowOutputs())
      if(wo.window)
        fpsBefore += wo.hasSwapChain ? 1 : 0;

    std::mt19937 rng{seed};
    std::uniform_int_distribution<int> pick{0, 2};
    for(int i = 0; i < nops; ++i)
    {
      const int which = pick(rng);
      log += (which == 0 ? "hideAll " : which == 1 ? "showAll " : "render ");
      for(const auto& wo : rig.node->windowOutputs())
      {
        if(!wo.window)
          continue;
        if(which == 0)
          wo.window->hide();
        else if(which == 1)
          wo.window->show();
      }
      pump_for(80);
      rig.render(2);
    }

    for(const auto& wo : rig.node->windowOutputs())
      if(wo.window)
        wo.window->show();
    pump_for(300);

    rig.render(30);
    for(const auto& wo : rig.node->windowOutputs())
      if(wo.window)
        fpsAfter += wo.hasSwapChain ? 1 : 0;
    canRenderAfter = rig.node->canRender();
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  INFO("backend=" << o.backend << " clock=" << clock.name << " seed=" << seed);
  INFO("sequence: " << log);
  REQUIRE(o.error.empty());
  INFO("live swap chains before=" << fpsBefore << " after=" << fpsAfter);
  REQUIRE(fpsBefore > 0);
  CHECK(canRenderAfter);
  CHECK(fpsAfter == fpsBefore);
}

TEST_CASE(
    "a background output recovers from any sequence of size changes",
    "[gfx][window][background][torture]")
{
  // BackgroundNode renders into a texture render target rather than a
  // swap chain, so it has no expose state to strand -- what can still go wrong
  // is a resize or a render-size override leaving the render list built against
  // the previous target. Readback validity is the recovery signal.
  const auto api = GENERATE(from_range(platform_backends()));
  const unsigned seed = seed_from_env();
  const int nops = ops_from_env();

  IsfResult r;
  std::string log;
  std::mt19937 rng{seed};
  std::uniform_int_distribution<int> dim{32, 320};

  for(int i = 0; i < nops / 4; ++i)
  {
    const QSize sz{dim(rng), dim(rng)};
    log += std::to_string(sz.width()) + "x" + std::to_string(sz.height()) + " ";
    run_in_gui_app([&](const score::GUIApplicationContext&) {
      r = render_isf_chain(
          api, {QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs")}, sz, 3);
    });
    if(r.skipped)
      SKIP(r.backend + ": " + r.skip_reason);
    INFO("sizes: " << log);
    REQUIRE(r.error.empty());
    REQUIRE(r.outputs.size() == 1);
    CHECK(r.outputs[0].valid());
  }
}
