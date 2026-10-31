// The ossia device trees of the window output device: Gfx/Window/WindowDevice.hpp
// and Gfx/Window/MultiWindowDevice.hpp.
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

#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Window/MultiWindowDevice.hpp>
#include <Gfx/WindowDevice.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QDateTime>
#include <QFile>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QMouseEvent>

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
  ossia::vec2f clipAfterMove{9.f, 9.f}, glAfterMove{9.f, 9.f};
  std::vector<ossia::value> pointsAfterMove, pointsAfterPress, pointsAfterRelease;
  bool historyIsTexture{};

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
         "/cursor/scaled", "/cursor/gl", "/cursor/clip", "/cursor/absolute",
         "/cursor/visible", "/touch/points", "/touch/history",
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

    // The pointer, as vertexshaderart reads it (main.js recordMouseMove /
    // recordMouseDown / recordMouseUp): a quarter of the way right and down
    // is (-0.5, 0.5) in clip space, y up; the button is pressure 1 on slot 0.
    {
      const QSizeF sz = screen->size();
      const QPointF at{sz.width() / 4., sz.height() / 4.};
      auto points = [&] {
        std::vector<ossia::value> r;
        if(auto* p = param(*d, "/touch/points"))
        {
          const ossia::value v = p->value();
          if(auto* l = v.target<std::vector<ossia::value>>())
            r = *l;
        }
        return r;
      };
      QMouseEvent move{
          QEvent::MouseMove, at, screen->mapToGlobal(at), Qt::NoButton, Qt::NoButton,
          Qt::NoModifier};
      QCoreApplication::sendEvent(screen, &move);
      pump_for(120);
      if(auto* p = param(*d, "/cursor/clip"))
      {
        const ossia::value cur = p->value();
        if(auto* v = cur.target<ossia::vec2f>())
          clipAfterMove = *v;
      }
      if(auto* p = param(*d, "/cursor/gl"))
      {
        const ossia::value cur = p->value();
        if(auto* v = cur.target<ossia::vec2f>())
          glAfterMove = *v;
      }
      pointsAfterMove = points();
      QMouseEvent press{
          QEvent::MouseButtonPress, at, screen->mapToGlobal(at), Qt::LeftButton,
          Qt::LeftButton, Qt::NoModifier};
      QCoreApplication::sendEvent(screen, &press);
      pump_for(120);
      pointsAfterPress = points();
      QMouseEvent release{
          QEvent::MouseButtonRelease, at, screen->mapToGlobal(at), Qt::LeftButton,
          Qt::NoButton, Qt::NoModifier};
      QCoreApplication::sendEvent(screen, &release);
      pump_for(120);
      pointsAfterRelease = points();
      if(auto* p = param(*d, "/touch/history"))
        historyIsTexture = p->get_type() == ossia::parameter_type::TEXTURE;
    }

    dev.disconnect();
    pump_for(120);
  });

  if(o.skipped)
    SKIP(o.skipReason);

  REQUIRE(connected);
  CHECK(clipAfterMove[0] == Approx(-0.5f).margin(0.02f));
  CHECK(clipAfterMove[1] == Approx(0.5f).margin(0.02f));
  CHECK(glAfterMove[0] == Approx(0.25f).margin(0.01f));
  CHECK(glAfterMove[1] == Approx(0.75f).margin(0.01f));
  auto slot0 = [](const std::vector<ossia::value>& pts) {
    ossia::vec3f r{9.f, 9.f, 9.f};
    if(!pts.empty())
      if(auto* v = pts[0].target<ossia::vec3f>())
        r = *v;
    return r;
  };
  CHECK(pointsAfterMove.size() == 32);
  CHECK(slot0(pointsAfterMove)[0] == Approx(-0.5f).margin(0.02f));
  CHECK(slot0(pointsAfterMove)[1] == Approx(0.5f).margin(0.02f));
  CHECK(slot0(pointsAfterMove)[2] == Approx(0.f));
  CHECK(slot0(pointsAfterPress)[2] == Approx(1.f));
  CHECK(slot0(pointsAfterRelease)[0] == Approx(-0.5f).margin(0.02f));
  CHECK(slot0(pointsAfterRelease)[2] == Approx(0.f));
  CHECK(historyIsTexture);
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
  CHECK(renderSizeAfter == QSize{200, 120});
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

TEST_CASE("Multi-window device warp, color and maps parameters", "[gfx][window][device]")
{
  Outcome o;
  bool connected{};
  std::vector<std::string> missing;
  QPointF tl, br;
  Gfx::OutputColor color;
  bool alphaLoaded{};
  float storedGain{};

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString alpha = dir.filePath("alpha.pfm");
  {
    score::gfx::FloatImage img;
    img.width = 4;
    img.height = 4;
    img.channels = 1;
    img.data.assign(16, 0.5f);
    REQUIRE(score::gfx::writePFM(alpha, img));
  }

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

    OutputMapping m;
    m.windowPosition = {20, 40};
    m.windowSize = {192, 144};
    m.cornerWarp.bottomRight = {0.9, 0.95};

    WindowSettings ws;
    ws.mode = WindowMode::MultiWindow;
    ws.outputs = {m};

    WindowDevice dev{makeSettings(ws, "MultiWinCal"), doc->context()};
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
        {"/0/warp/tl", "/0/warp/tr", "/0/warp/bl", "/0/warp/br", "/0/color/gain",
         "/0/color/offset", "/0/color/gamma", "/0/color/linear", "/0/maps/warp",
         "/0/maps/alpha", "/0/maps/beta"})
      if(!hasNode(*d, p))
        missing.emplace_back(p);

    auto* node = sinkOf<score::gfx::MultiWindowNode>(*d);
    REQUIRE(node);
    REQUIRE(!node->windowOutputs().empty());

    if(auto* p = param(*d, "/0/warp/br"))
    {
      const auto val = p->value();
      const auto v = val.target<ossia::vec2f>();
      CHECK(v);
      if(v)
        CHECK((*v)[0] == Approx(0.9f));
    }

    push(param(*d, "/0/warp/tl"), vec2(0.1f, 0.2f));
    push(param(*d, "/0/color/gain"), ossia::vec3f{0.5f, 0.6f, 0.7f});
    push(param(*d, "/0/color/offset"), ossia::vec3f{0.01f, 0.02f, 0.03f});
    push(param(*d, "/0/color/gamma"), 2.2f);
    push(param(*d, "/0/color/linear"), true);
    push(param(*d, "/0/maps/alpha"), alpha.toStdString());

    {
      const auto& wo = node->windowOutputs()[0];
      tl = wo.cornerWarp.topLeft;
      br = wo.cornerWarp.bottomRight;
      color = wo.color;
      alphaLoaded = wo.alphaMap && wo.alphaMap->width == 4;
      storedGain = node->mappings()[0].color.gain[1];
    }

    dev.disconnect();
    pump_for(200);
  });

  if(o.skipped)
    SKIP(o.skipReason);

  REQUIRE(connected);
  for(auto& m : missing)
    UNSCOPED_INFO("  missing " << m);
  CHECK(missing.empty());
  CHECK(tl.x() == Approx(0.1).margin(1e-4));
  CHECK(tl.y() == Approx(0.2).margin(1e-4));
  CHECK(br.x() == Approx(0.9).margin(1e-4));
  CHECK(br.y() == Approx(0.95).margin(1e-4));
  CHECK(color.gain[1] == Approx(0.6f));
  CHECK(color.offset[2] == Approx(0.03f));
  CHECK(color.gamma == Approx(2.2f));
  CHECK(color.linearBlend);
  CHECK(alphaLoaded);
  // Kept by the node across an output rebuild.
  CHECK(storedGain == Approx(0.6f));
}

TEST_CASE("Multi-window device locates map files and matches calibration suffixes", "[gfx][window][device][calibration]")
{
  score::PathRoots roots;
  roots.documentFile = QStringLiteral("/shows/hall/hall.score");
  OutputMaps m;
  m.alpha = QStringLiteral("cal/room.alpha.pfm");
  m.warp = QStringLiteral("<PROJECT>:/cal/room.mpcdi#proj1");
  m.lut = QStringLiteral("/abs/grade.cube");
  const auto l = locatedMaps(m, roots);
  CHECK(l.alpha == QStringLiteral("/shows/hall/cal/room.alpha.pfm"));
  CHECK(l.warp == QStringLiteral("/shows/hall/cal/room.mpcdi#proj1"));
  CHECK(l.lut == m.lut);
  CHECK(l.beta.isEmpty());

  const auto& slots = calibrationFileSlots();
  CHECK(isCalibrationFileOf("/a/room.ALPHA.pfm", "/a/room", slots[0]));
  CHECK(calibrationBase("/a/room.ALPHA.pfm") == "/a/room");
  CHECK_FALSE(isCalibrationFileOf("/a/Room.alpha.pfm", "/a/room", slots[0]));
  CHECK_FALSE(isCalibrationFileOf("/a/room.beta.pfm", "/a/room", slots[0]));
}

TEST_CASE("Multi-window device follows its calibration files on disk", "[gfx][window][device][calibration]")
{
  Outcome o;
  bool connected{};
  int alphaAtStart{}, alphaAfterRewrite{}, betaAfterCreate{}, alphaOther{};
  std::string alphaParam, calibrationParam;

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("room");
  const QString other = dir.filePath("other");
  auto writeGrey = [](const QString& path, int size, float v) {
    score::gfx::FloatImage img;
    img.width = size;
    img.height = size;
    img.channels = 1;
    img.data.assign(size * size, v);
    REQUIRE(score::gfx::writePFM(path, img));
  };
  writeGrey(base + ".alpha.pfm", 4, 0.5f);
  writeGrey(other + ".alpha.pfm", 6, 0.25f);
  {
    QFile f{base + ".json"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("{}");
  }

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

    OutputMapping m;
    m.windowPosition = {20, 40};
    m.windowSize = {160, 120};
    m.maps.calibration = base + ".json";

    WindowSettings ws;
    ws.mode = WindowMode::MultiWindow;
    ws.outputs = {m};

    WindowDevice dev{makeSettings(ws, "MultiWinLive"), doc->context()};
    if(!dev.reconnect())
    {
      o = {true, "the multi-window device could not connect"};
      return;
    }
    connected = true;
    auto* d = dev.getDevice();
    REQUIRE(d != nullptr);
    pump_for(400);
    auto* node = sinkOf<score::gfx::MultiWindowNode>(*d);
    REQUIRE(node);
    REQUIRE(!node->windowOutputs().empty());
    auto alphaWidth = [&] {
      const auto& a = node->windowOutputs()[0].alphaMap;
      return a ? a->width : 0;
    };
    auto betaWidth = [&] {
      const auto& b = node->windowOutputs()[0].betaMap;
      return b ? b->width : 0;
    };
    alphaAtStart = alphaWidth();

    // The watcher debounces 300 ms, and polls once a second without inotify.
    QFile::remove(base + ".alpha.pfm");
    pump_for(50);
    writeGrey(base + ".alpha.pfm", 8, 0.5f);
    pump_until([&] { return alphaWidth() == 8; }, 5000);
    alphaAfterRewrite = alphaWidth();

    writeGrey(base + ".beta.pfm", 5, 0.01f);
    pump_until([&] { return betaWidth() == 5; }, 5000);
    betaAfterCreate = betaWidth();

    push(param(*d, "/0/maps/calibration"), QString(other + ".json").toStdString());
    pump_for(200);
    alphaOther = alphaWidth();
    if(auto* p = param(*d, "/0/maps/alpha"))
      alphaParam = ossia::convert<std::string>(p->value());
    if(auto* p = param(*d, "/0/maps/calibration"))
      calibrationParam = ossia::convert<std::string>(p->value());

    dev.disconnect();
    pump_for(200);
  });

  if(o.skipped)
    SKIP(o.skipReason);
  REQUIRE(connected);
  CHECK(alphaAtStart == 4);
  CHECK(alphaAfterRewrite == 8);
  CHECK(betaAfterCreate == 5);
  CHECK(alphaOther == 6);
  CHECK(alphaParam == QString(other + ".alpha.pfm").toStdString());
  CHECK(calibrationParam == QString(other + ".json").toStdString());
}

TEST_CASE("A calibration fills only the slots its last run wrote", "[gfx][window][calibration]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("room");
  auto write = [](const QString& path, const QByteArray& bytes) {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(bytes);
  };
  auto setTime = [](const QString& path, const QDateTime& t) {
    QFile f{path};
    REQUIRE(f.open(QIODevice::ReadWrite));
    REQUIRE(f.setFileTime(t, QFileDevice::FileModificationTime));
  };
  for(const char* s : {".response.json", ".comp.json", ".alpha.pfm", ".beta.pfm", ".cube"})
    write(base + s, "x");
  write(base + ".json", R"({"files":["room.xyz.pfm","room.response.json"]})");
  write(base + ".blend.json", R"({"alphaMap":"room.alpha.pfm","betaMap":"room.beta.pfm"})");
  const auto t0 = QDateTime::currentDateTime().addSecs(-60);
  setTime(base + ".json", t0);
  setTime(base + ".blend.json", t0.addSecs(10));

  OutputMaps m;
  applyCalibration(m, base + ".json");
  CHECK(m.response == base + ".response.json");
  CHECK(m.compensation.isEmpty());
  CHECK(m.alpha == base + ".alpha.pfm");
  CHECK(m.beta == base + ".beta.pfm");
  CHECK(m.lut == base + ".cube");

  // A newer run without a blend: the old blend maps are no longer its own.
  setTime(base + ".json", t0.addSecs(20));
  CHECK(refreshCalibration(m));
  CHECK(m.alpha.isEmpty());
  CHECK(m.beta.isEmpty());
  CHECK(m.response == base + ".response.json");
  CHECK(m.compensation.isEmpty());

  // A calibration that lists nothing takes what it finds.
  write(base + ".json", "{}");
  CHECK(refreshCalibration(m));
  CHECK(m.compensation == base + ".comp.json");
  CHECK(m.alpha == base + ".alpha.pfm");
}
