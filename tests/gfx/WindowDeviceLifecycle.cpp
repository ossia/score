// The window output as the application actually assembles it: a real
// Gfx::WindowDevice in a real document, with the real GfxContext between the
// window and the graph.
//
// Every other windowed test in tests/gfx builds a ScreenNode directly and pumps
// frames itself. That skips the two pieces the reported failures live in --
// GfxContext's clock selection (the swap-chain vsync push loop vs the shared
// wall timer) and the device's own createOutput/destroyOutput cycle -- so a
// window that has stopped presenting in the running application still reads as
// healthy there. Reported symptom: close the output window, press Show, the
// viewport is black, and it stays black across stop/start; only disconnecting
// and reconnecting the device recovers it, while the process's preview widget
// keeps rendering normally the whole time (the preview is a separate render
// list on a separate timer, which is exactly why it is not evidence).
//
// The observable is Window::onRender: it runs only for a frame that reached the
// surface, so it answers "is this window's render loop alive" independently of
// what the graph is drawing.
//
// Registered GUI: these create presented windows and SKIP without a display.
//
// What these guard, and what they do not: reverting the registry release in
// ScreenNode's onWindowReady takes the close case down with a core dump, so
// that fix is pinned. Reverting the vsync render-gate refresh does NOT fail any
// of them, with either clock -- that fix was confirmed by hand in the running
// application and is still unguarded here.

#include "WindowedOutputCommon.hpp"

#include <Engine/ApplicationPlugin.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Settings/Model.hpp>

#include <Gfx/GfxParameter.hpp>

#include <JS/Qml/EditContext.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/WindowDevice.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <QCloseEvent>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace Gfx;
using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct Outcome
{
  bool skipped{};
  std::string skipReason;
  std::string backend;
};

Device::DeviceSettings windowSettings(QString name)
{
  Device::DeviceSettings s;
  s.name = std::move(name);
  s.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings ws;
  ws.mode = WindowMode::Single;
  s.deviceSpecificSettings = QVariant::fromValue(ws);
  return s;
}

/// Registers the window device with the document, the way the scripting API
/// does. A WindowDevice merely constructed on the stack is invisible to
/// setAddress(), so nothing can be routed to it and its output stays alone in
/// its render list -- which reads exactly like the bug under test.
WindowDevice* add_window_device(const score::DocumentContext& doc, QString name)
{
  auto& plug = doc.plugin<Explorer::DeviceDocumentPlugin>();
  Device::DeviceSettings set;
  set.name = name;
  set.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings ws;
  ws.mode = WindowMode::Single;
  set.deviceSpecificSettings = QVariant::fromValue(ws);

  CommandDispatcher<> disp{doc.commandStack};
  disp.submit(new Explorer::Command::LoadDevice{plug, std::move(set)});

  return dynamic_cast<WindowDevice*>(plug.list().findDevice(name));
}

/// The ScreenNode behind the device, reached the way WindowDevice::window() does.
score::gfx::ScreenNode* screen_of(WindowDevice& dev)
{
  if(auto* d = dev.getDevice())
    if(auto* p = d->get_root_node().get_parameter())
      if(auto* param = dynamic_cast<gfx_parameter_base*>(p))
        return dynamic_cast<score::gfx::ScreenNode*>(param->node);
  return nullptr;
}

/// Presses play the way the transport does. Without an execution there is no
/// render list for the window and no clock driving it, so nothing is presented
/// at all -- which is why every case here asserts a non-zero frame count BEFORE
/// the disruption it is testing.
/// Builds shader -> Window:/ through the same EditContext API the scripting
/// layer uses, then plays. Without a producer feeding the output there is no
/// render list for the window and no clock driving it, so nothing is presented
/// at all -- which is why every case here asserts a non-zero frame count BEFORE
/// the disruption under test.
/// Cube -> Model Display -> Window:/, the geometry path. A model renderer holds
/// a Mesh pointer into the RenderList's cache, so it is the family that a
/// mid-frame rebuild -- what a resize or a full-screen change triggers -- can
/// leave holding a freed mesh. A shader-only chain owns no geometry and cannot
/// reach any of it.
bool build_geometry_and_play(JS::EditJsContext& api, std::string& why)
{
  auto* itv = api.rootInterval();
  if(!itv)
  {
    why = "rootInterval() is null";
    return false;
  }
  // Array to mesh, not the Cube primitive: it emits DYNAMIC geometry, which is
  // what goes through RenderList::acquireMesh() into the custom-mesh cache the
  // rebuild deletes. A static primitive never touches that cache, so it cannot
  // reach the dangling-mesh path at all.
  auto* cube = api.createProcess(
      itv, QStringLiteral("dfc5bae9-c75c-4180-b4e8-be3063c8d8f2"), {});
  if(!cube)
  {
    why = "could not create Array to mesh";
    return false;
  }
  auto* display = api.createProcess(
      itv, QStringLiteral("9ce44e4b-eeb6-4042-bb7f-9d0b28190daf"), {});
  if(!display)
  {
    why = "could not create Model Display";
    return false;
  }
  auto* geomOut = api.outlet(cube, 0);
  auto* geomIn = api.inlet(display, 0);
  if(!geomOut || !geomIn)
  {
    why = "the cube or the display is missing its geometry port";
    return false;
  }
  api.createCable(geomOut, geomIn);

  auto* out = api.outlet(display, 0);
  if(!out)
  {
    why = "Model Display has no image outlet";
    return false;
  }
  api.setAddress(out, QStringLiteral("Win:/"));
  api.play();
  return true;
}

bool build_and_play(JS::EditJsContext& api, const QString& shader, std::string& why)
{
  auto* itv = api.rootInterval();
  if(!itv)
  {
    why = "rootInterval() is null";
    return false;
  }
  // The ISF process's concrete key, as the scripting tests pass it.
  auto* proc = api.createProcess(
      itv, QStringLiteral("74ca45ff-92c9-44a0-8f1a-754dea05ee1b"), shader);
  if(!proc)
  {
    why = "createProcess(Shader) returned null for " + shader.toStdString();
    return false;
  }
  auto* out = api.outlet(proc, 0);
  if(!out)
  {
    why = "the shader process has no outlet 0";
    return false;
  }
  api.setAddress(out, QStringLiteral("Win:/"));
  api.play();
  return true;
}

/// Counts frames that actually reached the surface, chaining whatever the node
/// installed rather than replacing it.
struct FrameCounter
{
  explicit FrameCounter(score::gfx::Window& w)
      : window{&w}
  {
    rearm();
  }
  ~FrameCounter()
  {
    if(window && m_tag.use_count() > 1)
      window->onRender = previous;
  }
  FrameCounter(const FrameCounter&) = delete;
  FrameCounter& operator=(const FrameCounter&) = delete;

  /// ScreenNode reassigns Window::onRender every time it (re)creates the
  /// output, which both a close and a re-show do: a counter installed once is
  /// silently replaced and then reads zero on a perfectly healthy window.
  ///
  /// Re-chaining blindly is worse -- it wraps our own wrapper and the chain
  /// recurses until the stack goes. The tag tells the two apart: while our
  /// lambda is installed it holds a copy, so use_count() > 1. Once the node has
  /// overwritten it the lambda is destroyed and the count drops back to one.
  void rearm()
  {
    if(!window || m_tag.use_count() > 1)
      return;
    previous = window->onRender;
    auto tag = m_tag;
    window->onRender = [this, tag](QRhiCommandBuffer& cb) {
      ++count;
      if(previous)
        previous(cb);
    };
  }

  int take()
  {
    const int n = count;
    count = 0;
    return n;
  }

  score::gfx::Window* window{};
  std::function<void(QRhiCommandBuffer&)> previous;
  int count{};

private:
  std::shared_ptr<int> m_tag = std::make_shared<int>(0);
};
}

TEST_CASE(
    "the window device's output keeps presenting across a close and a re-show",
    "[gfx][window][device][lifecycle]")
{
  Outcome o;
  int beforeClose{}, afterShow{};
  bool exposedAfter{}, connected{};

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

    auto* devPtr = add_window_device(doc->context(), QStringLiteral("Win"));
    if(!devPtr)
    {
      o = {true, "the window device could not be registered with the document"};
      return;
    }
    auto& dev = *devPtr;
    connected = true;

    // The ScreenNode exists as soon as the device connects, but its Window is
    // only built when GfxContext initialises the output.
    score::gfx::Window* windowPtr{};
    pump_until([&] { return (windowPtr = dev.window()) != nullptr; }, 5000);
    if(!windowPtr)
    {
      o = {true, "the device published no window"};
      return;
    }
    auto& win = *windowPtr;

    if(!pump_until([&] { return win.isExposed(); }, 5000))
    {
      o = {true, "the device's window never became exposed"};
      return;
    }

    JS::EditJsContext api;
    std::string why;
    if(!build_and_play(api, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"), why))
    {
      o = {true, "could not build a shader -> Win:/ graph: " + why};
      return;
    }

    // Let the graph settle first: the output is created during play, and that
    // is what installs the node's own onRender.
    pump_for(1500);
    FrameCounter frames{win};
    frames.rearm();
    pump_for(1000);
    beforeClose = frames.take();

    // The X button: QEvent::Close, which releases the swap chain from inside
    // Window::event() without going through the node's destroyOutput().
    QCloseEvent close;
    QCoreApplication::sendEvent(&win, &close);
    pump_for(300);

    // The context menu's "Show", which is Window::show() and nothing else.
    win.show();
    pump_until([&] { return win.isExposed(); }, 5000);
    exposedAfter = win.isExposed();

    frames.rearm();
    frames.take();
    pump_for(1500);
    afterShow = frames.take();
  });

  if(o.skipped)
    SKIP(o.skipReason);
  REQUIRE(connected);
  INFO("frames before close: " << beforeClose << ", after re-show: " << afterShow);
  // Negative control: without this the case would pass on a window that never
  // presented anything at all.
  REQUIRE(beforeClose > 0);
  CHECK(exposedAfter);
  CHECK(afterShow > 0);
}

TEST_CASE(
    "the window device's output keeps presenting across a hide and a re-show",
    "[gfx][window][device][lifecycle]")
{
  // The context menu's Hide/Show, which is Window::hide()/show() and nothing
  // more. Distinct from the close above: no swap chain is released and no new
  // device is built, so nothing rebuilds the render list -- what has to survive
  // is the flag Window::render() gates each frame on, which only the
  // timer-driven path used to refresh.
  Outcome o;
  int beforeHide{}, afterShow{};
  bool connected{}, exposedAfter{};

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
    auto* devPtr = add_window_device(doc->context(), QStringLiteral("Win"));
    if(!devPtr)
    {
      o = {true, "the window device could not be registered with the document"};
      return;
    }
    auto& dev = *devPtr;
    connected = true;

    score::gfx::Window* windowPtr{};
    pump_until([&] { return (windowPtr = dev.window()) != nullptr; }, 5000);
    if(!windowPtr)
    {
      o = {true, "the device published no window"};
      return;
    }
    auto& win = *windowPtr;
    if(!pump_until([&] { return win.isExposed(); }, 5000))
    {
      o = {true, "the device's window never became exposed"};
      return;
    }

    // The clock matters here: the timer path refreshes the render gate every
    // tick by itself, so only the vsync chain can strand it.
    ctx.settings<Gfx::Settings::Model>().setVSync(true);

    JS::EditJsContext api;
    std::string why;
    if(!build_and_play(api, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"), why))
    {
      o = {true, "could not build a shader -> Win:/ graph: " + why};
      return;
    }

    pump_for(1500);
    FrameCounter frames{win};
    frames.rearm();
    pump_for(1000);
    beforeHide = frames.take();

    win.hide();
    pump_until([&] { return !win.isExposed(); }, 3000);
    pump_for(300);

    win.show();
    pump_until([&] { return win.isExposed(); }, 5000);
    exposedAfter = win.isExposed();

    frames.rearm();
    frames.take();
    pump_for(1500);
    afterShow = frames.take();
  });

  if(o.skipped)
    SKIP(o.skipReason);
  REQUIRE(connected);
  INFO("frames before hide: " << beforeHide << ", after re-show: " << afterShow);
  REQUIRE(beforeHide > 0);
  CHECK(exposedAfter);
  CHECK(afterShow > 0);
}

TEST_CASE(
    "a geometry output survives resizes and full-screen changes",
    "[gfx][window][device][lifecycle][geometry]")
{
  // The reported crash: a document of point-cloud meshes, resized or sent full
  // screen, dies in RenderList::initMeshBuffer(). Both rebuild the render list
  // mid-frame through maybeRebuild(), which is release() followed straight by
  // init(), and the model renderer's Mesh pointer belongs to the list being
  // released. A Cube stands in for the point cloud: same geometry path, same
  // acquireMesh() pointer, no asset needed.
  Outcome o;
  int beforeResize{}, afterResize{};
  bool connected{};

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
    auto* devPtr = add_window_device(doc->context(), QStringLiteral("Win"));
    if(!devPtr)
    {
      o = {true, "the window device could not be registered with the document"};
      return;
    }
    auto& dev = *devPtr;
    connected = true;

    score::gfx::Window* windowPtr{};
    pump_until([&] { return (windowPtr = dev.window()) != nullptr; }, 5000);
    if(!windowPtr)
    {
      o = {true, "the device published no window"};
      return;
    }
    auto& win = *windowPtr;
    if(!pump_until([&] { return win.isExposed(); }, 5000))
    {
      o = {true, "the device's window never became exposed"};
      return;
    }

    JS::EditJsContext api;
    std::string why;
    if(!build_geometry_and_play(api, why))
    {
      o = {true, "could not build Cube -> Model Display -> Win:/ : " + why};
      return;
    }

    pump_for(2000);
    FrameCounter frames{win};
    frames.rearm();
    pump_for(1000);
    beforeResize = frames.take();

    // Sizes, odd extents and full screen both ways: the fast resize path and a
    // full rebuild take different code, and full screen resizes from the
    // platform side at a size nothing here chose.
    for(int i = 0; i < 8; ++i)
    {
      win.resize(200 + (i * 61) % 400, 150 + (i * 43) % 300);
      pump_for(120);
      frames.rearm();
      if(i % 2 == 0)
      {
        win.showFullScreen();
        pump_for(200);
        frames.rearm();
        win.showNormal();
        pump_for(200);
        frames.rearm();
      }
    }

    win.resize(320, 240);
    pump_for(400);
    frames.rearm();
    frames.take();
    pump_for(1200);
    afterResize = frames.take();
  });

  if(o.skipped)
    SKIP(o.skipReason);
  REQUIRE(connected);
  INFO("frames before resizing: " << beforeResize << ", after: " << afterResize);
  REQUIRE(beforeResize > 0);
  CHECK(afterResize > 0);
}

TEST_CASE(
    "the window device's output survives being closed twice",
    "[gfx][window][device][lifecycle]")
{
  // Whatever the first close/re-show leaves behind has to be recoverable too:
  // the reported failure persisted across stop/start, so a state that only
  // survives one cycle would still be a defect.
  Outcome o;
  int afterFirst{}, afterSecond{};
  bool connected{};

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
    auto* devPtr = add_window_device(doc->context(), QStringLiteral("Win"));
    if(!devPtr)
    {
      o = {true, "the window device could not be registered with the document"};
      return;
    }
    auto& dev = *devPtr;
    connected = true;
    // The ScreenNode exists as soon as the device connects, but its Window is
    // only built when GfxContext initialises the output.
    score::gfx::Window* windowPtr{};
    pump_until([&] { return (windowPtr = dev.window()) != nullptr; }, 5000);
    if(!windowPtr)
    {
      o = {true, "the device published no window"};
      return;
    }
    auto& win = *windowPtr;
    if(!pump_until([&] { return win.isExposed(); }, 5000))
    {
      o = {true, "the device's window never became exposed"};
      return;
    }

    JS::EditJsContext api;
    std::string why;
    if(!build_and_play(api, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"), why))
    {
      o = {true, "could not build a shader -> Win:/ graph: " + why};
      return;
    }
    pump_for(1500);
    FrameCounter frames{win};
    frames.rearm();
    pump_for(1000);
    if(frames.take() == 0)
    {
      o = {true, "the window presented nothing even before the close"};
      return;
    }
    for(int cycle = 0; cycle < 2; ++cycle)
    {
      QCloseEvent close;
      QCoreApplication::sendEvent(&win, &close);
      pump_for(250);
      win.show();
      pump_until([&] { return win.isExposed(); }, 5000);
      frames.rearm();
      frames.take();
      pump_for(1200);
      (cycle == 0 ? afterFirst : afterSecond) = frames.take();
    }
  });

  if(o.skipped)
    SKIP(o.skipReason);
  REQUIRE(connected);
  INFO("frames after first re-show: " << afterFirst
                                      << ", after second: " << afterSecond);
  CHECK(afterFirst > 0);
  CHECK(afterSecond > 0);
}
