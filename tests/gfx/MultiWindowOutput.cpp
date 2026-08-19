// L3 tests for score::gfx::MultiWindowNode: the multi-projector output that
// renders the upstream graph once into a stable offscreen target and then blits
// a per-window sub-region of it, with soft-edge blending, 4-corner warp and
// rotation/mirroring, into one presented swap chain per window.
//
// Everything here needs several *real* platform surfaces sharing one QRhi, so
// none of it is reachable from the offscreen fixture. Registered GUI (label
// "gui"); SKIPs cleanly with no windowing system.

#include "WindowedOutputCommon.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;
using Catch::Approx;

namespace
{
struct Outcome
{
  bool skipped{};
  std::string skipReason;
  std::string error;
  std::string backend;
};

/// Two side-by-side outputs carving the input in half, both small and placed
/// where a 1280x720-ish desktop can hold them.
std::vector<Gfx::OutputMapping> two_up()
{
  Gfx::OutputMapping left;
  left.sourceRect = {0.0, 0.0, 0.5, 1.0};
  left.windowPosition = {20, 40};
  left.windowSize = {192, 144};

  Gfx::OutputMapping right;
  right.sourceRect = {0.5, 0.0, 0.5, 1.0};
  right.windowPosition = {240, 40};
  right.windowSize = {192, 144};

  return {left, right};
}
}

TEST_CASE("MultiWindowNode brings up one swapchain per mapping",
          "[gfx][window][multiwindow]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  int windowCount{}, readyCount{}, windowsCreated{}, rendererCount{};
  bool offscreenReady{}, canRender{};
  QSize offscreenSize;
  QRectF rect0, rect1;

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    MultiWindowRig rig;
    if(!rig.build(api, two_up()))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(4);

    const auto& outs = rig.node->windowOutputs();
    windowCount = int(outs.size());
    for(auto& wo : outs)
      if(wo.hasSwapChain)
        ++readyCount;
    if(windowCount == 2)
    {
      rect0 = outs[0].sourceRect;
      rect1 = outs[1].sourceRect;
    }
    windowsCreated = rig.windowsCreatedCount;
    canRender = rig.node->canRender();
    offscreenReady = rig.node->offscreenTarget().texture != nullptr;
    if(offscreenReady)
      offscreenSize = rig.node->offscreenTarget().texture->pixelSize();
    if(auto* r = rig.node->renderer())
      rendererCount = int(r->renderers.size());
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(windowCount == 2);
  CHECK(readyCount == 2);
  CHECK(windowsCreated == 1);
  CHECK(canRender);
  CHECK(offscreenReady);
  CHECK(offscreenSize == QSize{1280, 720});
  CHECK(rendererCount >= 2);
  // The per-window source rects are seeded from the mappings once, in
  // createOutput; initWindowSwapChain must not clobber them.
  CHECK(rect0 == QRectF{0.0, 0.0, 0.5, 1.0});
  CHECK(rect1 == QRectF{0.5, 0.0, 0.5, 1.0});
}

TEST_CASE("MultiWindowNode live parameter setters", "[gfx][window][multiwindow]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  QRectF srcAfter;
  float bl{}, br{}, bt{}, bb{}, gl_{}, gr{}, gt{}, gb{};
  float blendAfterBadSide{-1.f};
  QPointF warpTL;
  int rotation{-1};
  bool mirrorX{}, mirrorY{};
  QRectF outOfRangeUnchanged;

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    MultiWindowRig rig;
    if(!rig.build(api, two_up()))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(2);

    rig.node->setSourceRect(0, {0.25, 0.1, 0.5, 0.8});
    rig.node->setEdgeBlend(0, 0, 0.10f, 1.5f);
    rig.node->setEdgeBlend(0, 1, 0.11f, 1.6f);
    rig.node->setEdgeBlend(0, 2, 0.12f, 1.7f);
    rig.node->setEdgeBlend(0, 3, 0.13f, 1.8f);
    // Out-of-range side index: must change nothing.
    rig.node->setEdgeBlend(0, 4, 0.99f, 9.9f);
    rig.node->setEdgeBlend(0, -1, 0.99f, 9.9f);
    // Out-of-range window index on every setter.
    rig.node->setSourceRect(7, {0.9, 0.9, 0.1, 0.1});
    rig.node->setSourceRect(-1, {0.9, 0.9, 0.1, 0.1});
    rig.node->setEdgeBlend(7, 0, 0.99f, 9.9f);
    rig.node->setCornerWarp(7, Gfx::CornerWarp{});
    rig.node->setTransform(7, 90, true, true);

    Gfx::CornerWarp warp;
    warp.topLeft = {0.05, 0.02};
    rig.node->setCornerWarp(0, warp);
    rig.node->setTransform(0, 180, true, false);

    rig.render(3);

    const auto& outs = rig.node->windowOutputs();
    srcAfter = outs[0].sourceRect;
    bl = outs[0].blendLeft.width;
    br = outs[0].blendRight.width;
    bt = outs[0].blendTop.width;
    bb = outs[0].blendBottom.width;
    gl_ = outs[0].blendLeft.gamma;
    gr = outs[0].blendRight.gamma;
    gt = outs[0].blendTop.gamma;
    gb = outs[0].blendBottom.gamma;
    blendAfterBadSide = outs[0].blendLeft.width;
    warpTL = outs[0].cornerWarp.topLeft;
    rotation = outs[0].rotation;
    mirrorX = outs[0].mirrorX;
    mirrorY = outs[0].mirrorY;
    outOfRangeUnchanged = outs[1].sourceRect;
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(srcAfter == QRectF{0.25, 0.1, 0.5, 0.8});
  CHECK(bl == Approx(0.10f));
  CHECK(br == Approx(0.11f));
  CHECK(bt == Approx(0.12f));
  CHECK(bb == Approx(0.13f));
  CHECK(gl_ == Approx(1.5f));
  CHECK(gr == Approx(1.6f));
  CHECK(gt == Approx(1.7f));
  CHECK(gb == Approx(1.8f));
  CHECK(blendAfterBadSide == Approx(0.10f));
  CHECK(warpTL == QPointF{0.05, 0.02});
  CHECK(rotation == 180);
  CHECK(mirrorX);
  CHECK_FALSE(mirrorY);
  // Window 1 was never addressed: the out-of-range writes must not have
  // fallen through onto it.
  CHECK(outOfRangeUnchanged == QRectF{0.5, 0.0, 0.5, 1.0});
}

TEST_CASE("MultiWindowNode render size recreates the offscreen target",
          "[gfx][window][multiwindow]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  QSize before, after, afterDegenerate, afterSame;
  bool sameTexture{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    MultiWindowRig rig;
    if(!rig.build(api, two_up()))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(2);
    before = rig.node->offscreenTarget().texture->pixelSize();

    rig.node->setRenderSize({640, 360});
    rig.render(3);
    after = rig.node->offscreenTarget().texture->pixelSize();

    // A degenerate size is rejected outright.
    rig.node->setRenderSize({0, 240});
    rig.node->setRenderSize({320, -1});
    afterDegenerate = rig.node->offscreenTarget().texture->pixelSize();

    // Re-setting the same size must not churn the target.
    auto* tex = rig.node->offscreenTarget().texture;
    rig.node->setRenderSize({640, 360});
    sameTexture = rig.node->offscreenTarget().texture == tex;
    afterSame = rig.node->offscreenTarget().texture->pixelSize();
    rig.render(2);
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(before == QSize{1280, 720});
  CHECK(after == QSize{640, 360});
  CHECK(afterDegenerate == QSize{640, 360});
  CHECK(sameTexture);
  CHECK(afterSame == QSize{640, 360});
}

TEST_CASE("MultiWindowNode window close and re-expose", "[gfx][window][multiwindow]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool otherStillReady{}, closedReleased{}, reacquired{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    MultiWindowRig rig;
    if(!rig.build(api, two_up()))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(3);

    auto* w0 = rig.node->windowOutputs()[0].window.get();
    QCloseEvent close;
    QCoreApplication::sendEvent(w0, &close);
    pump_for(150);

    closedReleased = !rig.node->windowOutputs()[0].hasSwapChain;
    otherStillReady = rig.node->windowOutputs()[1].hasSwapChain;
    // Rendering with one window down must keep serving the other.
    rig.render(3);

    w0->show();
    pump_until([&] { return rig.node->windowOutputs()[0].hasSwapChain; }, 5000);
    rig.render(3);
    reacquired = rig.node->windowOutputs()[0].hasSwapChain;
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(closedReleased);
  // The regression this pins: closing one output must not take the others
  // down with it.
  CHECK(otherStillReady);
  CHECK(reacquired);
}

TEST_CASE("MultiWindowNode renders black without a graph", "[gfx][window][multiwindow]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool survived{}, stillReady{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    MultiWindowRig rig;
    if(!rig.build(api, two_up()))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(2);

    // Drop the render list: MultiWindowNode::render() must fall back to
    // clearing every live window rather than dereferencing it.
    rig.node->setRenderer({});
    rig.render(3);
    survived = true;
    for(auto& wo : rig.node->windowOutputs())
      if(!wo.hasSwapChain)
        survived = false;
    stillReady = rig.node->canRender();
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(survived);
  CHECK(stillReady);
}

TEST_CASE("MultiWindowNode swapchain flag and format rebuild",
          "[gfx][window][multiwindow]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool before{}, afterSameFlag{}, afterFlag{}, afterFormat{}, rebuilt{};

  // No Graph here on purpose: destroyOutput() frees the QRhi, and a RenderList
  // built against it would then be released against a dead device. See
  // tests/gfx/ScreenOutputFindings.cpp.
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(!can_present())
    {
      o = {true, "no windowing system able to expose a native window", {},
           backend_name(api)};
      return;
    }
    std::string probed;
    if(!probe_api(api, probed))
    {
      o = {true, "backend unavailable", {}, backend_name(api)};
      return;
    }

    score::gfx::MultiWindowNode node{
        score::gfx::OutputNode::Configuration{
            .manualRenderingRate = 1000. / 60., .supportsVSync = false},
        two_up()};
    node.createOutput({.graphicsApi = api, .onReady = [] {}, .onResize = [] {}});
    before = bool(node.renderState());

    node.setSwapchainFlag(Gfx::SwapchainFlag::NoFlag);
    afterSameFlag = bool(node.renderState());

    node.setSwapchainFlag(Gfx::SwapchainFlag::sRGB);
    afterFlag = bool(node.renderState());

    node.setSwapchainFormat(Gfx::SwapchainFormat::HDR10);
    afterFormat = bool(node.renderState());

    // Coming back must honour both: the offscreen target is then RGBA32F
    // rather than RGBA8, and every swapchain carries the sRGB flag.
    node.createOutput({.graphicsApi = api, .onReady = [] {}, .onResize = [] {}});
    rebuilt = node.canRender()
              && node.offscreenTarget().texture->format() == QRhiTexture::RGBA32F;
    node.destroyOutput();
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  CHECK(before);
  CHECK(afterSameFlag);
  CHECK_FALSE(afterFlag);
  CHECK_FALSE(afterFormat);
  CHECK(rebuilt);
}

TEST_CASE("MultiWindowNode with no mappings does nothing", "[gfx][window][multiwindow]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool canRender{true}, hasState{true};
  int windows{-1};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(!can_present())
    {
      o = {true, "no windowing system able to expose a native window", {},
           backend_name(api)};
      return;
    }
    std::string probed;
    if(!probe_api(api, probed))
    {
      o = {true, "backend unavailable", {}, backend_name(api)};
      return;
    }

    score::gfx::MultiWindowNode node{
        score::gfx::OutputNode::Configuration{
            .manualRenderingRate = 1000. / 60., .supportsVSync = false},
        {}};
    node.createOutput({.graphicsApi = api});
    canRender = node.canRender();
    hasState = bool(node.renderState());
    windows = int(node.windowOutputs().size());
    // Rendering and tearing down an empty node must be inert, not a crash.
    node.render();
    node.destroyOutput();
    node.destroyOutput();
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  CHECK_FALSE(canRender);
  CHECK_FALSE(hasState);
  CHECK(windows == 0);
}
