// Resizing and full-screening a windowed output, with the object families that
// actually own GPU resources sized from the swap chain.
//
// The window suites next to this one render one solid-colour ISF, which owns a
// single pipeline and no geometry: a resize path can be badly wrong and still
// look healthy there. The reported crash comes from a document built out of
// compute shaders and point-cloud geometry, i.e. the nodes that carry meshes,
// storage images and preprocessor buffers across a rebuild, and it fires on
// resize and on entering or leaving full screen.
//
// A resize takes one of two paths -- RenderList::resizeSwapchainSizedTargets()
// for a size-only change, or a full recreateOutputRenderList() -- and they
// exercise completely different code. The sequences below mix sizes, full
// screen on and off, and render-size overrides so both are hit repeatedly and
// in varying order.
//
// Seeded: SCORE_TORTURE_SEED replays a sequence, SCORE_TORTURE_OPS sets length.
//
//   DISPLAY=:0 ctest -R gfx_window_resize_torture

#include "WindowedOutputCommon.hpp"

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

/// The shaders behind the reported document: a compute pass and a geometry
/// consumer alongside the plain fragment case, because they are what owns the
/// resources a resize has to carry or rebuild.
std::vector<QString> shader_set()
{
  // make_isf_node dispatches on the extension, so a .cs here is a compute node
  // with its own storage images -- the family the reported document is built
  // from, and the one whose resources a resize has to carry or rebuild.
  // Only the image-output computes: csf-storage-rw and csf-auxiliary-buffer
  // need a buffer producer upstream, which a bare source-to-window chain has
  // nowhere to put.
  return {
      QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"),
      QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-orient-quadrants.fs"),
      QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-gradient-y.cs"),
      QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-image-rgba16f.cs"),
      QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-image-r32f.cs"),
  };
}

enum class Op
{
  ResizeSmall,
  ResizeLarge,
  ResizeOdd,
  FullScreenOn,
  FullScreenOff,
  RenderSize,
  ClearRenderSize,
  Count
};

const char* op_name(Op o)
{
  switch(o)
  {
    case Op::ResizeSmall:
      return "resizeSmall";
    case Op::ResizeLarge:
      return "resizeLarge";
    case Op::ResizeOdd:
      return "resizeOdd";
    case Op::FullScreenOn:
      return "fullScreenOn";
    case Op::FullScreenOff:
      return "fullScreenOff";
    case Op::RenderSize:
      return "renderSize";
    case Op::ClearRenderSize:
      return "clearRenderSize";
    default:
      return "?";
  }
}
}

TEST_CASE(
    "a windowed output survives any sequence of resizes and full-screen changes",
    "[gfx][window][screen][resize][torture]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto shader = GENERATE(from_range(shader_set()));

  const unsigned seed = seed_from_env();
  const int nops = ops_from_env();

  Outcome o;
  std::string log;
  int presentedAfter{}, presentedBefore{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {320, 240}, shader))
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

    rig.render(10);
    presentedBefore = presented;

    std::mt19937 rng{seed};
    std::uniform_int_distribution<int> pick{0, int(Op::Count) - 1};
    std::uniform_int_distribution<int> small{16, 120};
    std::uniform_int_distribution<int> large{400, 900};
    for(int i = 0; i < nops; ++i)
    {
      const auto op = Op(pick(rng));
      log += op_name(op);
      log += ' ';
      switch(op)
      {
        case Op::ResizeSmall:
          win->resize(small(rng), small(rng));
          break;
        case Op::ResizeLarge:
          win->resize(large(rng), large(rng));
          break;
        case Op::ResizeOdd:
          // Odd and prime-ish extents: half-pixel viewports and mip rounding.
          win->resize(37 + (i * 13) % 200, 61 + (i * 7) % 150);
          break;
        case Op::FullScreenOn:
          rig.screen->setFullScreen(true);
          break;
        case Op::FullScreenOff:
          rig.screen->setFullScreen(false);
          break;
        case Op::RenderSize:
          rig.screen->setRenderSize(QSize{small(rng), small(rng)});
          break;
        case Op::ClearRenderSize:
          rig.screen->setRenderSize(QSize{});
          break;
        default:
          break;
      }
      // Both the fast resize path and a full rebuild need frames to run.
      rig.render(3);
      QCoreApplication::processEvents(QEventLoop::AllEvents, 4);
    }

    rig.screen->setFullScreen(false);
    rig.screen->setRenderSize(QSize{});
    win->resize(320, 240);
    pump_for(200);

    presented = 0;
    rig.render(20);
    presentedAfter = presented;

    win->onRender = inner;
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  INFO("backend=" << o.backend << " shader=" << shader.toStdString()
                  << " seed=" << seed << " (SCORE_TORTURE_SEED to replay)");
  INFO("sequence: " << log);
  REQUIRE(o.error.empty());
  // Negative control: the sequence proves nothing if the rig never rendered.
  REQUIRE(presentedBefore > 0);
  CHECK(presentedAfter > 0);
}

TEST_CASE(
    "a windowed output survives full screen entered and left repeatedly",
    "[gfx][window][screen][resize][torture]")
{
  // Full screen is its own case because it changes the surface size from the
  // platform side rather than through resize(), and back again: the swap chain
  // is recreated at a size nothing in score chose.
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  int presentedAfter{}, presentedBefore{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {256, 192}))
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

    rig.render(10);
    presentedBefore = presented;

    for(int i = 0; i < 6; ++i)
    {
      rig.screen->setFullScreen(true);
      pump_for(150);
      rig.render(3);
      rig.screen->setFullScreen(false);
      pump_for(150);
      rig.render(3);
    }

    pump_for(200);
    presented = 0;
    rig.render(20);
    presentedAfter = presented;

    win->onRender = inner;
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  INFO("backend: " << o.backend);
  REQUIRE(o.error.empty());
  REQUIRE(presentedBefore > 0);
  CHECK(presentedAfter > 0);
}
