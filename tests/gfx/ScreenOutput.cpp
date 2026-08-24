// L3 tests for the *presented* single-window output: score::gfx::ScreenNode and
// the score::gfx::Window it owns.
//
// These are the paths that only run when there is a real platform surface with
// a swap chain: expose / hide / re-expose, swap-chain (re)creation and resize,
// the render-size override, the manual-FPS and vsync drivers, the input event
// fan-out, the device-lost latch and the swapchain-flag / graphics-API rebuild.
// The whole rest of tests/gfx/ renders offscreen and cannot reach any of it.
//
// Requires a windowing system: registered GUI (label "gui"), and every case
// SKIPs cleanly when no native window can be exposed.

#include "WindowedOutputCommon.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;
using Catch::Approx;

namespace
{
// A run's outcome, collected inside run_in_gui_app and asserted after it
// returns (a REQUIRE inside the lambda would unwind past the app teardown).
struct Outcome
{
  bool skipped{};
  std::string skipReason;
  std::string error;
  std::string backend;
};
}

TEST_CASE("ScreenNode presents a swapchain", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool exposed{}, canRender{}, hasRenderState{}, hasRenderer{};
  QSize outputSize, swapSize, renderSize;
  int rendererCount{};
  int fpsCount{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {256, 192}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();

    rig.render(4);

    exposed = rig.window()->isExposed();
    canRender = rig.screen->canRender();
    if(auto rs = rig.screen->renderState())
    {
      hasRenderState = true;
      outputSize = rs->outputSize;
      renderSize = rs->renderSize;
    }
    if(auto* r = rig.screen->renderer())
    {
      hasRenderer = true;
      rendererCount = int(r->renderers.size());
    }
    swapSize = rig.window()->size();
    fpsCount = rig.fpsCount;
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(exposed);
  CHECK(canRender);
  REQUIRE(hasRenderState);
  // The window was asked for 256x192; the swap chain's pixel size is what the
  // RenderState reports as the output size (modulo device pixel ratio).
  CHECK(outputSize.width() > 0);
  CHECK(outputSize.height() > 0);
  // No explicit render size was set, so the ScreenNode's onResize copies the
  // output size into the render size. That is the whole point of the callback:
  // without it the render list would stay at the 1280x720 seed.
  CHECK(renderSize == outputSize);
  REQUIRE(hasRenderer);
  // producer + sink: ScreenNode::onRendererChange only enables m_canRender when
  // the render list holds more than the sink itself.
  CHECK(rendererCount >= 2);
  // startRendering() pushes an initial 0 and each frame pushes at most one
  // value every 50ms; at minimum the initial one must have arrived.
  CHECK(fpsCount >= 1);
}

TEST_CASE("ScreenNode window resize rebuilds the swapchain", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  QSize before, after, renderAfter;
  bool stillRendering{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {256, 192}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(3);
    before = rig.screen->renderState()->outputSize;

    rig.screen->setSize({384, 288});
    // The platform delivers the resize asynchronously; Window::render() then
    // notices currentPixelSize() != surfacePixelSize() and resizes the chain.
    pump_until(
        [&] {
      rig.render(1);
      return rig.screen->renderState()->outputSize != before;
        },
        5000);
    rig.render(3);

    after = rig.screen->renderState()->outputSize;
    renderAfter = rig.screen->renderState()->renderSize;
    stillRendering = rig.screen->canRender();
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend << " " << before.width() << "x" << before.height()
                   << " -> " << after.width() << "x" << after.height());
  CHECK(after != before);
  CHECK(after.width() > before.width());
  CHECK(after.height() > before.height());
  // Window::resizeSwapChain writes the new swapchain size into the render
  // state, and ScreenNode's onResize mirrors it into renderSize.
  CHECK(renderAfter == after);
  CHECK(stillRendering);
}

TEST_CASE("ScreenNode render size overrides the swapchain size", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  QSize output, overridden, restored, negative;
  int resizeCallbacks{};

  // BareScreenRig on purpose: this case is about the setter's own two branches
  // (a valid size sets the override, a degenerate one clears it), so it asserts
  // on the node with nothing else able to write RenderState::renderSize.
  //
  // It used to carry a second reason -- under a live Graph the override was
  // immediately overwritten by resizeSwapchainSizedTargets(outputSize), so a
  // Graph-backed assertion could not tell the two branches apart and stayed
  // green with the guard deleted. That is no longer true: the fast path takes
  // the render size from the RenderState the output node has already updated,
  // and ScreenOutputFindings.cpp asserts the override across a real resize
  // under a live Graph.
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    BareScreenRig rig;
    if(!rig.build(api, {256, 192}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    output = rig.screen->renderState()->outputSize;

    const int before = rig.resizeCount;
    rig.screen->setRenderSize({96, 64});
    overridden = rig.screen->renderState()->renderSize;
    resizeCallbacks = rig.resizeCount - before;

    // A degenerate size clears the override, so the render size follows the
    // swapchain again.
    rig.screen->setRenderSize({0, 0});
    restored = rig.screen->renderState()->renderSize;

    rig.screen->setRenderSize({-4, 8});
    negative = rig.screen->renderState()->renderSize;
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(output.width() > 0);
  CHECK(overridden == QSize{96, 64});
  CHECK(restored == output);
  CHECK(negative == output);
  // setRenderSize forwards to the window's onResize, which forwards to the
  // owner's.
  CHECK(resizeCallbacks == 1);
}

TEST_CASE("ScreenNode survives hide and re-show", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool renderedBefore{}, renderedWhileHidden{}, renderedAfter{}, exposedAfter{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {256, 192}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(3);
    renderedBefore = rig.screen->canRender();

    rig.window()->hide();
    pump_until([&] { return !rig.window()->isExposed(); }, 3000);
    rig.render(3);
    renderedWhileHidden = rig.screen->canRender();

    rig.window()->show();
    pump_until([&] { return rig.window()->isExposed(); }, 5000);
    rig.render(3);
    exposedAfter = rig.window()->isExposed();
    renderedAfter = rig.screen->canRender();
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(renderedBefore);
  CHECK(exposedAfter);
  // The regression this pins: after a hide/show cycle the window must be able
  // to render again. A latched m_notExposed leaves it black forever.
  CHECK(renderedAfter);
}

TEST_CASE("a re-shown window presents frames again", "[gfx][window][screen]")
{
  // "ScreenNode survives hide and re-show" above asserts canRender(), which is
  // only `m_window && m_hasSwapChain`: it stays true while render() early-returns
  // on a latched m_notExposed, so it cannot see a window that has stopped
  // presenting. The user-visible symptom -- hide, show, and the output is dead
  // until the device is disconnected and reconnected -- is counted here off
  // Window::onRender, which runs only for a frame that actually reached the
  // surface.
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  int before{}, after{};
  bool exposedAfter{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api))
    {
      o.skipped = !rig.skipReason().empty();
      o.skipReason = rig.skipReason();
      o.error = rig.error();
      return;
    }
    o.backend = rig.backend();

    auto* win = rig.window();
    REQUIRE(win != nullptr);

    // Chain, never replace: ScreenNode's own onRender is what draws the graph.
    int presented = 0;
    auto inner = win->onRender;
    win->onRender = [&presented, inner](QRhiCommandBuffer& cb) {
      ++presented;
      if(inner)
        inner(cb);
    };

    rig.render(30);
    before = presented;

    win->hide();
    pump_until([&] { return !win->isExposed(); }, 3000);
    rig.render(10);

    win->show();
    pump_until([&] { return win->isExposed(); }, 5000);
    exposedAfter = win->isExposed();

    presented = 0;
    rig.render(30);
    after = presented;

    win->onRender = inner;
  });

  if(o.skipped)
    SKIP(o.skipReason);
  INFO("backend: " << o.backend);
  REQUIRE(o.error.empty());
  INFO("presented before hide: " << before << ", after re-show: " << after);
  REQUIRE(before > 0);
  CHECK(exposedAfter);
  CHECK(after > 0);
}

TEST_CASE("a closed and re-shown window presents frames again",
          "[gfx][window][screen]")
{
  // Same counter as above, but the window goes away through the Close path
  // rather than hide(): QEvent::Close (and SurfaceAboutToBeDestroyed, which
  // falls through to it) releases the swapchain and latches m_running=false,
  // m_hasSwapChain=false, m_notExposed=true, m_closed=true all at once.
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  int before{}, after{};
  bool exposedAfter{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api))
    {
      o.skipped = !rig.skipReason().empty();
      o.skipReason = rig.skipReason();
      o.error = rig.error();
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

    rig.render(30);
    before = presented;

    QCloseEvent close;
    QCoreApplication::sendEvent(win, &close);
    pump_for(200);

    win->show();
    pump_until([&] { return win->isExposed(); }, 5000);
    exposedAfter = win->isExposed();

    presented = 0;
    rig.render(30);
    after = presented;

    win->onRender = inner;
  });

  if(o.skipped)
    SKIP(o.skipReason);
  INFO("backend: " << o.backend);
  REQUIRE(o.error.empty());
  INFO("presented before close: " << before << ", after re-show: " << after);
  REQUIRE(before > 0);
  INFO("exposed after re-show: " << exposedAfter);
  CHECK(after > 0);
}

TEST_CASE("Window swapchain release and re-create", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool afterRelease{}, afterResize{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {256, 192}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(2);

    rig.window()->releaseSwapChain();
    afterRelease = rig.screen->canRender();

    rig.window()->resizeSwapChain();
    afterResize = rig.screen->canRender();
    rig.render(2);
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK_FALSE(afterRelease);
  CHECK(afterResize);
}

TEST_CASE("Window device loss stops the output", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool lost{}, canRenderAfter{};
  int deviceLostCallbacks{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {256, 192}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(2);

    rig.window()->onDeviceLost = [&] { ++deviceLostCallbacks; };
    rig.window()->handleDeviceLost();
    // A second call must not re-arm the callback.
    rig.window()->handleDeviceLost();
    pump_for(120);

    lost = rig.window()->deviceLost();
    canRenderAfter = rig.screen->canRender();
    // ScreenNode::render() must return before touching the dead context.
    rig.render(2);
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(lost);
  CHECK_FALSE(canRenderAfter);
  CHECK(deviceLostCallbacks == 1);
}

TEST_CASE("Window forwards input events", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  int keys{}, keyReleases{}, mouseMoves{}, interactive{};
  int lastKey{-1};
  QString lastText;

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {256, 192}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();

    rig.screen->onKey = [&](int k, const QString& t) {
      ++keys;
      lastKey = k;
      lastText = t;
    };
    rig.screen->onKeyRelease = [&](int, const QString&) { ++keyReleases; };
    rig.screen->onMouseMove = [&](QPointF, QPointF) { ++mouseMoves; };
    QObject::connect(
        rig.window(), &score::gfx::Window::interactiveEvent, rig.window(),
        [&](QEvent*) { ++interactive; });

    auto* w = rig.window();
    {
      QKeyEvent press{QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a")};
      QCoreApplication::sendEvent(w, &press);
      QKeyEvent rel{QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, QStringLiteral("a")};
      QCoreApplication::sendEvent(w, &rel);
    }
    {
      // Auto-repeat is filtered out on both press and release.
      QKeyEvent rep{
          QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"), true};
      QCoreApplication::sendEvent(w, &rep);
      QKeyEvent repRel{
          QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"), true};
      QCoreApplication::sendEvent(w, &repRel);
    }
    {
      QMouseEvent move{QEvent::MouseMove,   QPointF{10, 20}, QPointF{10, 20},
                       Qt::NoButton,        Qt::NoButton,    Qt::NoModifier};
      QCoreApplication::sendEvent(w, &move);
      QMouseEvent press{QEvent::MouseButtonPress, QPointF{10, 20}, QPointF{10, 20},
                        Qt::LeftButton,           Qt::LeftButton,  Qt::NoModifier};
      QCoreApplication::sendEvent(w, &press);
      QMouseEvent release{QEvent::MouseButtonRelease, QPointF{10, 20}, QPointF{10, 20},
                          Qt::LeftButton,             Qt::NoButton,    Qt::NoModifier};
      QCoreApplication::sendEvent(w, &release);
    }
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(keys == 1);
  CHECK(keyReleases == 1);
  CHECK(lastKey == int(Qt::Key_A));
  CHECK(lastText == QStringLiteral("a"));
  CHECK(mouseMoves == 1);
  // key press + key release + mouse move + press + release
  CHECK(interactive == 5);
}

TEST_CASE("Window swallows deferred deletion", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool aliveAfter{}, stillRendering{}, swallowed{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {192, 144}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(2);

    // The Window is owned by a shared_ptr, never by the QObject tree: honouring
    // a DeferredDelete would `delete this` on the interior pointer of a
    // make_shared block. It must be swallowed.
    //
    // deleteLater() + processEvents() does NOT reach Window::event: Qt only
    // flushes DeferredDelete when the event loop that posted it unwinds, so a
    // pumped test cannot tell the two branches apart (verified — it stayed
    // green with the swallow deleted). Deliver the event synchronously
    // instead. A QPointer is the probe: it nulls itself iff the QObject was
    // really destroyed, without dereferencing freed memory.
    QPointer<score::gfx::Window> alive{rig.window()};
    QEvent del{QEvent::DeferredDelete};
    swallowed = rig.window()->event(&del);

    aliveAfter = !alive.isNull();
    rig.render(2);
    stillRendering = rig.screen->canRender();
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(swallowed);
  CHECK(aliveAfter);
  CHECK(stillRendering);
}

TEST_CASE("ScreenNode swapchain flag and format rebuild the output",
          "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool stateBefore{}, sameFlagKeptState{}, stateAfterFlag{}, stateAfterFormat{};
  bool rebuilt{};
  int readyCount{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    BareScreenRig rig;
    if(!rig.build(api))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    stateBefore = bool(rig.screen->renderState());

    // Re-setting the same value must not tear anything down.
    rig.screen->setSwapchainFlag(Gfx::SwapchainFlag::NoFlag);
    sameFlagKeptState = bool(rig.screen->renderState());

    rig.screen->setSwapchainFlag(Gfx::SwapchainFlag::sRGB);
    stateAfterFlag = bool(rig.screen->renderState());

    // The window is gone now (destroyOutput resets it), so the format setter
    // takes its "no window" branch but still records the value.
    rig.screen->setSwapchainFormat(Gfx::SwapchainFormat::HDR10);
    stateAfterFormat = bool(rig.screen->renderState());

    // Bringing the output back must honour the new flag + format.
    rig.screen->createOutput(
        {.graphicsApi = api, .onReady = [&] { ++readyCount; }, .onResize = [] {}});
    rebuilt = pump_until([&] { return rig.screen->canRender(); }, 5000);
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(stateBefore);
  CHECK(sameFlagKeptState);
  // setSwapchainFlag on a live output routes through destroyOutput().
  CHECK_FALSE(stateAfterFlag);
  CHECK_FALSE(stateAfterFormat);
  CHECK(rebuilt);
  CHECK(readyCount == 1);
}

TEST_CASE("ScreenNode graphics-API change destroys the output", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool before{}, afterSame{}, afterOther{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    BareScreenRig rig;
    if(!rig.build(api))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    before = bool(rig.screen->renderState());

    rig.screen->updateGraphicsAPI(api);
    afterSame = bool(rig.screen->renderState());

    const auto other
        = (api == score::gfx::OpenGL) ? score::gfx::Vulkan : score::gfx::OpenGL;
    rig.screen->updateGraphicsAPI(other);
    afterOther = bool(rig.screen->renderState());
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(before);
  // Same API + same sample count: nothing is torn down.
  CHECK(afterSame);
  CHECK_FALSE(afterOther);
}

TEST_CASE("ScreenNode start and stop rendering", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool rendererAfterStop{};
  int fpsAfterStop{};
  float lastFpsValue{-1.f};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {192, 144}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    rig.render(3);

    const int fpsBefore = rig.fpsCount;
    rig.screen->stopRendering();
    fpsAfterStop = rig.fpsCount - fpsBefore;
    lastFpsValue = rig.lastFps;
    rendererAfterStop = rig.screen->renderer() != nullptr;

    // Rendering after a stop must not crash: the render list weak_ptr is gone.
    rig.render(2);
    rig.screen->startRendering();
    rig.render(2);
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  // stopRendering pushes a 0 fps and drops the render list.
  CHECK(fpsAfterStop == 1);
  CHECK(lastFpsValue == Approx(0.f));
  CHECK_FALSE(rendererAfterStop);
}

TEST_CASE("ScreenNode vsync callback drives the window", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  int ticks{};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(
           api, {192, 144}, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"),
           {.manualRenderingRate = {}, .supportsVSync = true}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();

    // Arming the callback from a null state must kick a first frame by itself:
    // the vsync loop is a self-perpetuating requestUpdate() chain and nothing
    // else restarts it (the "render freezes until I move the window" bug).
    rig.screen->setVSyncCallback([&] { ++ticks; });
    pump_until([&] { return ticks >= 3; }, 3000);

    rig.screen->setVSyncCallback({});
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(ticks >= 3);
}

TEST_CASE("the vsync loop survives a hide and re-show", "[gfx][window][screen]")
{
  // The app drives a single windowed output off the swap-chain vsync callback,
  // a self-perpetuating requestUpdate() chain. Qt delivers no UpdateRequest to a
  // window that is not exposed, so hiding breaks the chain, and only
  // Window::exposeEvent() can restart it -- it calls render() just once, and
  // only when `isExposed() && !surfaceSize.isEmpty()`, where surfaceSize is
  // QSize{} whenever m_hasSwapChain is false.
  //
  // Everything above pumps frames by hand through rig.render(), which keeps a
  // dead loop looking alive. Here nothing is pumped after the re-show: the ticks
  // must come from the window itself.
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  int afterShow{}, beforeHide{};
  bool exposedAfter{};
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(
           api, {192, 144}, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"),
           {.manualRenderingRate = {}, .supportsVSync = true}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();
    auto* win = rig.window();
    REQUIRE(win != nullptr);

    int ticks = 0;
    rig.screen->setVSyncCallback([&] { ++ticks; });
    pump_until([&] { return ticks >= 3; }, 3000);
    beforeHide = ticks;

    win->hide();
    pump_until([&] { return !win->isExposed(); }, 3000);
    pump_for(200);

    win->show();
    pump_until([&] { return win->isExposed(); }, 5000);
    exposedAfter = win->isExposed();

    // Nothing drives the graph from here: only the window's own loop can.
    ticks = 0;
    pump_for(1500);
    afterShow = ticks;

    rig.screen->setVSyncCallback({});
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  INFO("backend: " << o.backend);
  INFO("ticks before hide: " << beforeHide << ", after re-show: " << afterShow);
  REQUIRE(beforeHide >= 3);
  CHECK(exposedAfter);
  CHECK(afterShow > 0);
}

TEST_CASE("ScreenNode cursor, title and position setters", "[gfx][window][screen]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  Outcome o;
  bool blankByDefault{}, unsetAfterTrue{}, blankAfterFalse{};
  QString title;
  QPoint pos;

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {192, 144}))
    {
      o = {rig.skipped(), rig.skipReason(), rig.error(), rig.backend()};
      return;
    }
    o.backend = rig.backend();

    blankByDefault = rig.window()->cursor().shape() == Qt::BlankCursor;
    rig.screen->setCursor(true);
    unsetAfterTrue = rig.window()->cursor().shape() != Qt::BlankCursor;
    rig.screen->setCursor(false);
    blankAfterFalse = rig.window()->cursor().shape() == Qt::BlankCursor;
    // Idempotent both ways.
    rig.screen->setCursor(false);

    rig.screen->setTitle(QStringLiteral("renamed"));
    title = rig.window()->title();

    rig.screen->setPosition({60, 70});
    pump_for(200);
    pos = rig.window()->position();

    if(!QGuiApplication::screens().isEmpty())
      rig.screen->setScreen(QGuiApplication::screens().front());
  });

  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());

  INFO("backend: " << o.backend);
  CHECK(blankByDefault);
  CHECK(unsetAfterTrue);
  CHECK(blankAfterFalse);
  CHECK(title == QStringLiteral("renamed"));
  CHECK(pos == QPoint{60, 70});
}
