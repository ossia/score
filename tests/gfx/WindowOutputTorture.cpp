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

#include <algorithm>
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

/// The order the operations are performed in: every one of them once, shuffled,
/// then uniformly random for the remaining length.
///
/// Drawing uniformly from the start looks equivalent and is not. Full screen was
/// absent from this file's operation set entirely -- the enum went straight from
/// clearRenderSize to raise -- and the suite stayed green because no run had to
/// contain it. Covering the set by construction is what makes an operation that
/// is declared but never dispatched fail rather than pass quietly.
std::vector<int> op_order(int count, int nops, std::mt19937& rng)
{
  std::vector<int> seq(count);
  for(int i = 0; i < count; ++i)
    seq[i] = i;
  std::shuffle(seq.begin(), seq.end(), rng);

  std::uniform_int_distribution<int> pick{0, count - 1};
  while(int(seq.size()) < nops)
    seq.push_back(pick(rng));
  seq.resize(std::max(nops, count));
  return seq;
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
  FullScreenOn,
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
    case Op::FullScreenOn:
      return "fullScreenOn";
    case Op::FullScreenOff:
      return "fullScreenOff";
    case Op::Raise:
      return "raise";
    default:
      return "?";
  }
}

/// The multi-window equivalents. Hide and show apply to every window at once —
/// that is what a user does to the whole output — while the disruptions that
/// desynchronise the windows from each other are aimed at one of them, since a
/// node holding N swap chains has to recover each independently.
enum class MwOp
{
  HideAll,
  ShowAll,
  ResizeOne,
  CloseOne,
  FullScreenOne,
  NormalOne,
  RenderSize,
  ClearRenderSize,
  Count
};

const char* mw_op_name(MwOp o)
{
  switch(o)
  {
    case MwOp::HideAll:
      return "hideAll";
    case MwOp::ShowAll:
      return "showAll";
    case MwOp::ResizeOne:
      return "resizeOne";
    case MwOp::CloseOne:
      return "closeOne";
    case MwOp::FullScreenOne:
      return "fullScreenOne";
    case MwOp::NormalOne:
      return "normalOne";
    case MwOp::RenderSize:
      return "renderSize";
    case MwOp::ClearRenderSize:
      return "clearRenderSize";
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
    const auto seq = op_order(int(Op::Count), nops, rng);
    for(int i = 0; i < int(seq.size()); ++i)
    {
      const auto op = Op(seq[i]);
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
        case Op::FullScreenOn:
          rig.screen->setFullScreen(true);
          pump_for(150);
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
    rig.screen->setFullScreen(false);
    rig.screen->setRenderSize(QSize{});
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
    const auto seq = op_order(int(MwOp::Count), nops, rng);
    std::uniform_int_distribution<int> dim{64, 480};
    for(int i = 0; i < int(seq.size()); ++i)
    {
      const auto op = MwOp(seq[i]);
      const auto& outs = rig.node->windowOutputs();
      const int target = outs.empty() ? 0 : int(i % outs.size());
      log += mw_op_name(op);
      log += ' ';
      for(int w = 0; w < int(outs.size()); ++w)
      {
        auto* win = outs[w].window.get();
        if(!win)
          continue;
        switch(op)
        {
          case MwOp::HideAll:
            win->hide();
            break;
          case MwOp::ShowAll:
            win->show();
            break;
          case MwOp::ResizeOne:
            if(w == target)
              win->resize(dim(rng), dim(rng));
            break;
          case MwOp::CloseOne:
            if(w == target)
            {
              QCloseEvent close;
              QCoreApplication::sendEvent(win, &close);
            }
            break;
          case MwOp::FullScreenOne:
            if(w == target)
              win->showFullScreen();
            break;
          case MwOp::NormalOne:
            if(w == target)
              win->showNormal();
            break;
          default:
            break;
        }
      }
      // Render size is a node-level property, not a per-window one.
      if(op == MwOp::RenderSize)
        rig.node->setRenderSize(QSize{dim(rng), dim(rng)});
      else if(op == MwOp::ClearRenderSize)
        rig.node->setRenderSize(QSize{});

      pump_for(80);
      rig.render(2);
    }

    // Recovery: every window back to a plain visible state at the node's own
    // render size, which is what a user leaving full screen ends up with.
    rig.node->setRenderSize(QSize{});
    for(const auto& wo : rig.node->windowOutputs())
      if(wo.window)
      {
        wo.window->showNormal();
        wo.window->show();
      }
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
