// The ossia device trees of the window output device: Gfx/Window/WindowDevice.hpp
// (406 branches, 0%) and Gfx/Window/MultiWindowDevice.hpp (420, 0%).
//
// These headers are where the score::gfx nodes become remote-controllable: every
// window property (screen, position, size, fullscreen, cursor, render size, per
// output source rect / soft-edge blend) is an ossia parameter with a callback
// onto the ScreenNode / MultiWindowNode, and the input events travel the other
// way (mouse, tablet, keyboard -> parameters). Nothing of that is reachable
// without building the real device through the real protocol factory.
//
// Registered GUI: the devices create presented windows, so this needs a display
// and SKIPs cleanly without one.

#include "WindowedOutputCommon.hpp"

#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/WindowDevice.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QKeyEvent>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace Gfx;
using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct Outcome
{
  bool skipped{};
  std::string skipReason;
};

Device::DeviceSettings makeSettings(WindowSettings ws, QString name)
{
  Device::DeviceSettings s;
  s.name = std::move(name);
  s.protocol = WindowProtocolFactory::static_concreteKey();
  s.deviceSpecificSettings = QVariant::fromValue(ws);
  return s;
}

ossia::net::parameter_base* param(ossia::net::device_base& dev, const char* path)
{
  auto* n = ossia::net::find_node(dev.get_root_node(), path);
  return n ? n->get_parameter() : nullptr;
}

bool hasNode(ossia::net::device_base& dev, const char* path)
{
  return ossia::net::find_node(dev.get_root_node(), path) != nullptr;
}

/// Push a value and let the device's run_async marshalling and the platform
/// catch up.
void push(ossia::net::parameter_base* p, const ossia::value& v)
{
  REQUIRE(p != nullptr);
  p->push_value(v);
  pump_for(120);
}

ossia::vec2f vec2(float a, float b)
{
  return ossia::vec2f{a, b};
}

/// The score::gfx sink the device's root parameter points at — the same hop
/// WindowDevice::window() takes to reach the ScreenNode.
template <typename T>
T* sinkOf(ossia::net::device_base& d)
{
  auto* p = d.get_root_node().get_parameter();
  if(auto* gp = dynamic_cast<gfx_parameter_base*>(p))
    return dynamic_cast<T*>(gp->node);
  return nullptr;
}
}

TEST_CASE("Window device parameter tree", "[gfx][window][device]")
{
  Outcome o;
  bool connected{};
  std::vector<std::string> missing;
  QPoint posAfter;
  QSize sizeAfter;
  bool cursorBlankAfter{}, cursorShownAfter{};
  QSize renderSizeAfter;
  int keyCode{-1};
  std::string keyText;
  int releaseCode{-1};
  ossia::vec2f posParamAfterMove{};

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(!can_present())
    {
      o = {true, "no windowing system able to expose a native window"};
      return;
    }
    auto* doc = score::test::new_document(ctx);
    if(!doc)
    {
      o = {true, "no document delegate"};
      return;
    }

    WindowSettings ws;
    ws.mode = WindowMode::Single;
    WindowDevice dev{makeSettings(ws, "Win"), doc->context()};
    if(!dev.reconnect())
    {
      o = {true, "the window device could not connect (no gfx document plugin?)"};
      return;
    }
    connected = true;
    auto* d = dev.getDevice();
    REQUIRE(d != nullptr);
    pump_for(300);

    // The published address surface.
    for(const char* p :
        {"/screen", "/position", "/size", "/rendersize", "/fullscreen", "/fps",
         "/cursor/scaled", "/cursor/gl", "/cursor/absolute", "/cursor/visible",
         "/tablet/scaled", "/tablet/absolute", "/tablet/z", "/tablet/pressure",
         "/tablet/tangential", "/tablet/rotation", "/tablet/tilt_x", "/tablet/tilt_y",
         "/key/press/code", "/key/press/text", "/key/release/code",
         "/key/release/text"})
      if(!hasNode(*d, p))
        missing.emplace_back(p);

    auto* screen = dev.window();
    if(!screen)
    {
      o = {true, "the device has no window (offscreen fallback)"};
      return;
    }

    push(param(*d, "/size"), vec2(360.f, 260.f));
    sizeAfter = screen->size();

    push(param(*d, "/position"), vec2(180.f, 140.f));
    posAfter = screen->position();

    // The window's own move must travel back into the parameter (the
    // lock-guarded round trip).
    screen->setPosition({240, 200});
    pump_for(200);
    if(auto* p = param(*d, "/position"))
    {
      const ossia::value cur = p->value();
      if(auto* v = cur.target<ossia::vec2f>())
        posParamAfterMove = *v;
    }

    push(param(*d, "/cursor/visible"), false);
    cursorBlankAfter = screen->cursor().shape() == Qt::BlankCursor;
    push(param(*d, "/cursor/visible"), true);
    cursorShownAfter = screen->cursor().shape() != Qt::BlankCursor;

    // NOTE: /rendersize is asserted in ScreenOutputFindings.cpp — under a live
    // Graph it is currently inert. Pushed here anyway so the callback runs.
    push(param(*d, "/rendersize"), vec2(200.f, 120.f));
    if(auto* sn = sinkOf<score::gfx::ScreenNode>(*d))
      if(auto st = sn->renderState())
        renderSizeAfter = st->renderSize;

    // Input travelling the other way.
    {
      QKeyEvent press{QEvent::KeyPress, Qt::Key_F5, Qt::NoModifier, QStringLiteral("x")};
      QCoreApplication::sendEvent(screen, &press);
      QKeyEvent rel{QEvent::KeyRelease, Qt::Key_F5, Qt::NoModifier, QStringLiteral("x")};
      QCoreApplication::sendEvent(screen, &rel);
      pump_for(120);
    }
    if(auto* p = param(*d, "/key/press/code"))
    {
      const ossia::value cur = p->value();
      if(auto* v = cur.target<int>())
        keyCode = *v;
    }
    if(auto* p = param(*d, "/key/press/text"))
    {
      const ossia::value cur = p->value();
      if(auto* v = cur.target<std::string>())
        keyText = *v;
    }
    if(auto* p = param(*d, "/key/release/code"))
    {
      const ossia::value cur = p->value();
      if(auto* v = cur.target<int>())
        releaseCode = *v;
    }

    dev.disconnect();
    pump_for(120);
  });

  if(o.skipped)
    SKIP(o.skipReason);

  REQUIRE(connected);
  INFO("missing addresses: " << missing.size());
  for(auto& m : missing)
    UNSCOPED_INFO("  missing " << m);
  CHECK(missing.empty());

  CHECK(sizeAfter == QSize{360, 260});
  CHECK(posAfter == QPoint{180, 140});
  CHECK(posParamAfterMove[0] == Approx(240.f).margin(1.f));
  CHECK(posParamAfterMove[1] == Approx(200.f).margin(1.f));
  CHECK(cursorBlankAfter);
  CHECK(cursorShownAfter);
  CHECK(renderSizeAfter.width() > 0);
  CHECK(keyCode == int(Qt::Key_F5));
  CHECK(keyText == "x");
  CHECK(releaseCode == int(Qt::Key_F5));
}

TEST_CASE("Multi-window device parameter tree", "[gfx][window][device]")
{
  Outcome o;
  bool connected{};
  std::vector<std::string> missing;
  int windowCount{};
  QRectF src0, src1;
  float blendW{}, blendG{}, blendW2{}, blendG2{};
  QSize offscreenAfter;
  QSize win0SizeAfter;

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(!can_present())
    {
      o = {true, "no windowing system able to expose a native window"};
      return;
    }
    auto* doc = score::test::new_document(ctx);
    if(!doc)
    {
      o = {true, "no document delegate"};
      return;
    }

    OutputMapping left, right;
    left.sourceRect = {0.0, 0.0, 0.5, 1.0};
    left.windowPosition = {20, 40};
    left.windowSize = {192, 144};
    right.sourceRect = {0.5, 0.0, 0.5, 1.0};
    right.windowPosition = {240, 40};
    right.windowSize = {192, 144};

    WindowSettings ws;
    ws.mode = WindowMode::MultiWindow;
    ws.outputs = {left, right};

    WindowDevice dev{makeSettings(ws, "MultiWin"), doc->context()};
    if(!dev.reconnect())
    {
      o = {true, "the multi-window device could not connect"};
      return;
    }
    connected = true;
    auto* d = dev.getDevice();
    REQUIRE(d != nullptr);
    pump_for(400);

    for(const char* p :
        {"/fps", "/rendersize", "/0/cursor/scaled", "/0/cursor/gl", "/0/size",
         "/0/position", "/0/fullscreen", "/0/screen", "/0/source/position",
         "/0/source/size", "/0/blend/left/width", "/0/blend/left/gamma",
         "/0/blend/right/width", "/0/blend/top/width", "/0/blend/bottom/width",
         "/0/key/press/code", "/1/size", "/1/source/position", "/1/blend/left/width"})
      if(!hasNode(*d, p))
        missing.emplace_back(p);
    // Exactly two per-window subtrees.
    CHECK_FALSE(hasNode(*d, "/2/size"));

    auto* node = sinkOf<score::gfx::MultiWindowNode>(*d);
    if(!node)
    {
      o = {true, "the device is not in multi-window mode"};
      return;
    }
    windowCount = int(node->windowOutputs().size());

    // Source rect: position and size are separate parameters onto the same rect.
    push(param(*d, "/0/source/position"), vec2(0.25f, 0.10f));
    push(param(*d, "/0/source/size"), vec2(0.40f, 0.60f));
    if(windowCount == 2)
    {
      src0 = node->windowOutputs()[0].sourceRect;
      src1 = node->windowOutputs()[1].sourceRect;
    }

    // Blend: width and gamma are separate parameters and must not clobber
    // each other.
    push(param(*d, "/0/blend/left/width"), 0.125f);
    push(param(*d, "/0/blend/left/gamma"), 1.75f);
    if(windowCount >= 1)
    {
      blendW = node->windowOutputs()[0].blendLeft.width;
      blendG = node->windowOutputs()[0].blendLeft.gamma;
    }
    push(param(*d, "/0/blend/bottom/gamma"), 3.0f);
    push(param(*d, "/0/blend/bottom/width"), 0.0625f);
    if(windowCount >= 1)
    {
      blendW2 = node->windowOutputs()[0].blendBottom.width;
      blendG2 = node->windowOutputs()[0].blendBottom.gamma;
    }

    push(param(*d, "/rendersize"), vec2(640.f, 360.f));
    if(auto* t = node->offscreenTarget().texture)
      offscreenAfter = t->pixelSize();

    push(param(*d, "/1/size"), vec2(260.f, 200.f));
    if(windowCount == 2)
      if(auto& w = node->windowOutputs()[1].window)
        win0SizeAfter = w->size();

    dev.disconnect();
    pump_for(200);
  });

  if(o.skipped)
    SKIP(o.skipReason);

  REQUIRE(connected);
  for(auto& m : missing)
    UNSCOPED_INFO("  missing " << m);
  CHECK(missing.empty());
  REQUIRE(windowCount == 2);

  CHECK(src0.x() == Approx(0.25).margin(0.001));
  CHECK(src0.y() == Approx(0.10).margin(0.001));
  CHECK(src0.width() == Approx(0.40).margin(0.001));
  CHECK(src0.height() == Approx(0.60).margin(0.001));
  // Window 1 was not addressed and keeps its mapping.
  CHECK(src1.x() == Approx(0.5).margin(0.001));
  CHECK(src1.width() == Approx(0.5).margin(0.001));

  CHECK(blendW == Approx(0.125f).margin(0.001));
  // The gamma push must have carried the width over rather than resetting it.
  CHECK(blendG == Approx(1.75f).margin(0.001));
  CHECK(blendW2 == Approx(0.0625f).margin(0.001));
  CHECK(blendG2 == Approx(3.0f).margin(0.001));

  CHECK(offscreenAfter == QSize{640, 360});
  CHECK(win0SizeAfter == QSize{260, 200});
}
