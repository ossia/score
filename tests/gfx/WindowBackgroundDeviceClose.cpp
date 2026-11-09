// A document with a Window device in Background mode closes cleanly.
//
// The background device draws into the document's scenario view and listens
// to that view's signals (size, hover, tablet, keys). Taking its renderer off
// the view changes the view's background mode, which can resize the view: a
// sizeChanged delivered while the device is being destroyed reaches a size
// parameter whose device is half gone. Closing such a document crashed in
// ~background_device every time.
//
// Covered: a device added to a fresh document, and the same document saved
// and loaded back (the path a user opening a .score takes), each closed with
// and without a shader rendering into it, and the view resized while the
// device is alive so the size connection is known to be live. A device removed
// from the tree of a document that stays open leaves no slot on the view: the
// view's signals have as many receivers as before the device was added, which
// a slot outliving its parameters would break with or without a sanitizer.

#include "WindowedOutputCommon.hpp"

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Gfx/GfxParameter.hpp>
#include <Gfx/Window/BackgroundDevice.hpp>
#include <Gfx/WindowDevice.hpp>

#include <JS/Qml/EditContext.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/DocumentView.hpp>

#include <ossia/network/base/node_functions.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QFileInfo>
#include <QProcess>
#include <QScreen>
#include <QWindow>

using namespace Gfx;
using namespace score::test;
using namespace score::test::gfx;

namespace
{
WindowDevice* add_window_device(
    const score::DocumentContext& doc, QString name, WindowMode mode)
{
  auto& plug = doc.plugin<Explorer::DeviceDocumentPlugin>();
  Device::DeviceSettings set;
  set.name = name;
  set.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings ws;
  ws.mode = mode;
  ws.inputWidth = 1920;
  ws.inputHeight = 1080;
  set.deviceSpecificSettings = QVariant::fromValue(ws);

  CommandDispatcher<> disp{doc.commandStack};
  disp.submit(new Explorer::Command::LoadDevice{plug, std::move(set)});

  return dynamic_cast<WindowDevice*>(plug.list().findDevice(name));
}

WindowDevice* add_background_device(const score::DocumentContext& doc, QString name)
{
  return add_window_device(doc, std::move(name), WindowMode::Background);
}

Scenario::ScenarioDocumentView* scenario_view(score::Document& doc)
{
  if(auto* v = doc.view())
    return qobject_cast<Scenario::ScenarioDocumentView*>(&v->viewDelegate());
  return nullptr;
}

ossia::net::parameter_base* size_parameter(WindowDevice& dev)
{
  if(auto* d = dev.getDevice())
    if(auto* n = ossia::net::find_node(d->get_root_node(), "/size"))
      return n->get_parameter();
  return nullptr;
}

// Receivers of each view signal the background device connects to.
std::array<int, 5> view_receivers(const Scenario::ProcessGraphicsView& v)
{
  // QObject::receivers is protected; a pointer to it named through a derived
  // class is still a pointer to QObject's member.
  struct Peek : QObject
  {
    using QObject::receivers;
  };
  constexpr auto receivers = &Peek::receivers;
  const auto count = [&](QMetaMethod m) {
    return (v.*receivers)(QByteArray("2" + m.methodSignature()).constData());
  };
  using V = Scenario::ProcessGraphicsView;
  return {
      count(QMetaMethod::fromSignal(&V::sizeChanged)),
      count(QMetaMethod::fromSignal(&V::hoverMove)),
      count(QMetaMethod::fromSignal(&V::keyPress)),
      count(QMetaMethod::fromSignal(&V::keyRelease)),
      count(QMetaMethod::fromSignal(&V::tabletMove))};
}

Device::Node* device_node(const score::DocumentContext& doc, const QString& name)
{
  auto& plug = doc.plugin<Explorer::DeviceDocumentPlugin>();
  for(auto& n : plug.rootNode())
    if(n.get<Device::DeviceSettings>().name == name)
      return &n;
  return nullptr;
}

struct Outcome
{
  bool skipped{};
  std::string skipReason;
  bool sizeFollowedView{};
};
}

TEST_CASE(
    "a document with a Background window device closes without crashing",
    "[gfx][window][device][lifecycle][background]")
{
  const bool reload = GENERATE(false, true);
  const bool render = GENERATE(false, true);
  CAPTURE(reload, render);

  Outcome o;
  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    if(!doc)
    {
      o = {true, "no document delegate"};
      return;
    }
    if(!scenario_view(*doc))
    {
      o = {true, "the document has no scenario view to draw a background in"};
      return;
    }

    auto* dev = add_background_device(doc->context(), QStringLiteral("Win"));
    REQUIRE(dev);
    REQUIRE(dev->getDevice());

    if(reload)
    {
      auto* reloaded = score::test::reload_via_json(ctx, *doc);
      REQUIRE(reloaded);
      ctx.docManager.forceCloseDocument(ctx, *doc);
      QApplication::processEvents();
      doc = reloaded;
      auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
      dev = dynamic_cast<WindowDevice*>(plug.list().findDevice(QStringLiteral("Win")));
      REQUIRE(dev);
      REQUIRE(dev->getDevice());
    }

    auto* view = scenario_view(*doc);
    REQUIRE(view);

    if(render)
    {
      JS::EditJsContext api;
      auto* itv = api.rootInterval();
      REQUIRE(itv);
      auto* proc = api.createProcess(
          itv, QStringLiteral("74ca45ff-92c9-44a0-8f1a-754dea05ee1b"),
          QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"));
      REQUIRE(proc);
      auto* out = api.outlet(proc, 0);
      REQUIRE(out);
      api.setAddress(out, QStringLiteral("Win:/"));
      api.play();
      pump_for(500);
    }

    // The size connection is live: a resize of the view reaches the device.
    auto& gv = view->view();
    if(auto* w = gv.window())
    {
      w->show();
      w->resize(900, 700);
    }
    pump_for(100);
    if(auto* p = size_parameter(*dev))
    {
      const auto before = p->value();
      gv.resize(gv.width() + 37, gv.height() + 23);
      pump_for(100);
      o.sizeFollowedView = p->value() != before;
    }

    if(render)
    {
      JS::EditJsContext api;
      api.stop();
      pump_for(200);
    }

    ctx.docManager.forceCloseDocument(ctx, *doc);
    pump_for(200);
  });

  if(o.skipped)
    SKIP(o.skipReason);
  CHECK(o.sizeFollowedView);
}

TEST_CASE(
    "a removed Background window device leaves no slot on the view",
    "[gfx][window][device][lifecycle][background]")
{
  bool skipped{};
  std::string skipReason;
  std::array<int, 5> before{}, alive{}, after{};
  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    if(!doc || !scenario_view(*doc))
    {
      skipped = true;
      skipReason = "no document with a scenario view";
      return;
    }
    auto& gv = scenario_view(*doc)->view();
    before = view_receivers(gv);

    auto* dev = add_background_device(doc->context(), QStringLiteral("Win"));
    REQUIRE(dev);
    REQUIRE(dev->getDevice());
    pump_for(100);
    alive = view_receivers(gv);

    auto* node = device_node(doc->context(), QStringLiteral("Win"));
    REQUIRE(node);
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Explorer::Command::Remove{
        doc->context().plugin<Explorer::DeviceDocumentPlugin>(), *node});
    pump_for(100);
    after = view_receivers(gv);

    // Reaches a freed parameter if a slot survived.
    gv.resize(gv.width() + 11, gv.height() + 7);
    pump_for(50);

    ctx.docManager.forceCloseDocument(ctx, *doc);
    pump_for(200);
  });

  if(skipped)
    SKIP(skipReason);
  CAPTURE(before, alive, after);
  // The device listens to the view while it is alive, so the counts below are
  // the ones that matter.
  CHECK(alive[0] > before[0]);
  CHECK(after == before);
}

TEST_CASE(
    "SCORE_FORCE_BACKGROUND_WINDOW makes single windows background ones",
    "[gfx][window][device][background]")
{
  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    if(!doc || !scenario_view(*doc))
      SKIP("the document has no scenario view to draw a background in");

    const auto settings_mode = [](WindowDevice& dev) {
      return dev.settings().deviceSpecificSettings.value<WindowSettings>().mode;
    };

    qputenv("SCORE_FORCE_BACKGROUND_WINDOW", "1");
    auto* forced = add_window_device(doc->context(), QStringLiteral("Forced"), WindowMode::Single);
    qunsetenv("SCORE_FORCE_BACKGROUND_WINDOW");
    REQUIRE(forced);
    REQUIRE(forced->getDevice());
    CHECK(dynamic_cast<background_device*>(forced->getDevice()));
    // The document keeps what it was set to
    CHECK(settings_mode(*forced) == WindowMode::Single);

    // Without it, a single window is a window
    auto* plain = add_window_device(doc->context(), QStringLiteral("Plain"), WindowMode::Single);
    REQUIRE(plain);
    REQUIRE(plain->getDevice());
    CHECK(!dynamic_cast<background_device*>(plain->getDevice()));

    ctx.docManager.forceCloseDocument(ctx, *doc);
    QApplication::processEvents();
  });
}

// A Background window device renders at the size of the view it draws in.
//
// Its "size" node is saved with the document like every other node, and the
// saved tree is put back into the device when the document or the device is
// loaded. That value is the size the view had on the machine and the window
// layout the document was saved from: it is not the size of the view the
// device draws in now. The device takes its size from that view, the moment
// it is created, so restoring the tree must not change it.
//
// "size" is in device-independent pixels, as the view's size is; the picture
// is rendered at the screen's resolution, the view's size times its device
// pixel ratio, as a window's swapchain is.
//
// Covered: the paths that put a saved tree back into a device - a document or
// .device file being loaded, a removal being undone after the view was
// resized - with the view already laid out, so that no resize comes after to
// hide a wrong size. Then a resize of the view, which the device follows. All
// of it again at a device pixel ratio of 2, in a child process started with
// QT_SCALE_FACTOR=2.
namespace
{
Device::Node saved_background_device(const QString& name, ossia::vec2f size)
{
  Device::DeviceSettings set;
  set.name = name;
  set.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings ws;
  ws.mode = WindowMode::Background;
  set.deviceSpecificSettings = QVariant::fromValue(ws);

  Device::Node node{set, nullptr};
  Device::AddressSettings as;
  as.name = QStringLiteral("size");
  as.value = size;
  as.ioType = ossia::access_mode::BI;
  node.emplace_back(std::move(as), nullptr);
  return node;
}

struct DeviceSizes
{
  qreal dpr{};
  QSize view;
  QSize pixels;
  QSize parameter;
  QSize node;
  QSize renderTarget;
};

DeviceSizes device_sizes(Scenario::ScenarioDocumentView& view, WindowDevice& dev)
{
  DeviceSizes s;
  s.dpr = view.view().devicePixelRatioF();
  s.view = view.view().size();
  s.pixels = QSize(qRound(s.view.width() * s.dpr), qRound(s.view.height() * s.dpr));
  if(auto* p = size_parameter(dev))
  {
    const auto v = p->value();
    if(auto sz = v.target<ossia::vec2f>())
      s.parameter = QSize(int((*sz)[0]), int((*sz)[1]));
  }
  if(auto* bd = dynamic_cast<background_device*>(dev.getDevice()))
  {
    s.node = bd->node()->size();
    // The output is created when the graph first takes the node in
    QElapsedTimer t;
    t.start();
    while(!bd->node()->currentRenderTarget().texture && t.elapsed() < 2000)
      pump_for(10);
    if(auto* tex = bd->node()->currentRenderTarget().texture)
      s.renderTarget = tex->pixelSize();
  }
  return s;
}

void show_laid_out(Scenario::ScenarioDocumentView& view)
{
  if(auto* w = view.view().window())
  {
    w->show();
    w->resize(900, 700);
  }
  pump_for(200);
}

std::string to_string(QSize s)
{
  return std::to_string(s.width()) + "x" + std::to_string(s.height());
}
}

namespace Catch
{
template <>
struct StringMaker<DeviceSizes>
{
  static std::string convert(const DeviceSizes& s)
  {
    return "dpr " + std::to_string(s.dpr) + ", view " + to_string(s.view) + " ("
           + to_string(s.pixels) + " pixels), parameter " + to_string(s.parameter)
           + ", node " + to_string(s.node) + ", render target "
           + to_string(s.renderTarget);
  }
};
}

TEST_CASE(
    "a Background window device renders at its view's size, not at the size it was saved with",
    "[gfx][window][device][background][size]")
{
  const auto path = GENERATE(as<std::string>{}, "load", "undo removal");
  CAPTURE(path);

  bool skipped{};
  DeviceSizes loaded{}, resized{};
  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    if(!doc || !scenario_view(*doc))
    {
      skipped = true;
      return;
    }
    auto& view = *scenario_view(*doc);
    auto& gv = view.view();
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    show_laid_out(view);

    if(path == "load")
    {
      // What a document or a .device file saved with another view size holds
      CommandDispatcher<> disp{doc->context().commandStack};
      disp.submit(new Explorer::Command::LoadDevice{
          plug, saved_background_device(QStringLiteral("Win"), {300.f, 200.f})});
    }
    else
    {
      auto* dev = add_background_device(doc->context(), QStringLiteral("Win"));
      REQUIRE(dev);
      pump_for(100);
      gv.resize(gv.width() + 37, gv.height() + 23);
      pump_for(100);

      auto* node = device_node(doc->context(), QStringLiteral("Win"));
      REQUIRE(node);
      CommandDispatcher<> disp{doc->context().commandStack};
      disp.submit(new Explorer::Command::Remove{plug, *node});
      pump_for(100);

      gv.resize(gv.width() - 61, gv.height() - 41);
      pump_for(100);
      doc->commandStack().undo();
    }
    pump_for(100);

    auto* dev = dynamic_cast<WindowDevice*>(plug.list().findDevice(QStringLiteral("Win")));
    REQUIRE(dev);
    REQUIRE(dynamic_cast<background_device*>(dev->getDevice()));
    loaded = device_sizes(view, *dev);

    gv.resize(gv.width() + 53, gv.height() + 29);
    pump_for(100);
    resized = device_sizes(view, *dev);

    ctx.docManager.forceCloseDocument(ctx, *doc);
    pump_for(200);
  });

  if(skipped)
    SKIP("the document has no scenario view to draw a background in");

  CAPTURE(loaded, resized);
  if(const auto expected = qgetenv("SCORE_TEST_EXPECT_DPR"); !expected.isEmpty())
    REQUIRE(loaded.dpr == expected.toDouble());

  CHECK(loaded.parameter == loaded.view);
  CHECK(loaded.node == loaded.pixels);
  CHECK(loaded.renderTarget == loaded.pixels);

  CHECK(resized.view != loaded.view);
  CHECK(resized.parameter == resized.view);
  CHECK(resized.node == resized.pixels);
  CHECK(resized.renderTarget == resized.pixels);
}

TEST_CASE(
    "a Background window device renders at its view's size at a device pixel ratio of 2",
    "[gfx][window][device][background][hidpi]")
{
  const QString self = QFileInfo{QStringLiteral("/proc/self/exe")}.canonicalFilePath();
  if(self.isEmpty())
    SKIP("cannot start this test binary again: /proc/self/exe is unavailable");

  auto env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("2"));
  env.insert(QStringLiteral("QT_SCREEN_SCALE_FACTORS"), QString{});
  env.insert(QStringLiteral("SCORE_TEST_EXPECT_DPR"), QStringLiteral("2"));

  QProcess child;
  child.setProcessEnvironment(env);
  child.setProcessChannelMode(QProcess::MergedChannels);
  child.start(self, {QStringLiteral("[size]")});
  REQUIRE(child.waitForStarted(30000));
  const bool finished = child.waitForFinished(300000);
  if(!finished)
    child.kill();
  const auto log = QString::fromUtf8(child.readAll()).toStdString();
  INFO(log);
  REQUIRE(finished);
  REQUIRE(child.exitStatus() == QProcess::NormalExit);
  CHECK(child.exitCode() == 0);
}

// The device pixel ratio changes without a resize when score's window moves to
// a screen of another density, and the picture follows it as it follows a
// resize.
//
// Covered: the window moved from a screen at a device pixel ratio of 1 to one
// at 2, in a child process: the parent splits its X screen into two RandR
// monitors and the child gives them their ratios with QT_SCREEN_SCALE_FACTORS.
// Where that cannot be set up, what the move delivers instead: the view's
// DevicePixelRatioChange event and its window's screenChanged signal, each on
// a picture rendered at a size that no longer matches.
namespace
{
constexpr auto kMoveSkip = "DPR-MOVE-SKIP";
constexpr auto kMoveDone = "DPR-MOVE-DONE";

void say(const char* marker, const std::string& detail = {})
{
  std::printf("%s %s\n", marker, detail.c_str());
  std::fflush(stdout);
}
}

TEST_CASE(
    "a Background window device follows a device pixel ratio change without a resize",
    "[gfx][window][device][background][dpr]")
{
  bool skipped{};
  DeviceSizes initial{}, afterEvent{}, afterSignal{};
  QSize staleEvent, staleSignal;
  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    if(!doc || !scenario_view(*doc))
    {
      skipped = true;
      return;
    }
    auto& view = *scenario_view(*doc);
    auto& gv = view.view();
    show_laid_out(view);

    auto* dev = add_background_device(doc->context(), QStringLiteral("Win"));
    REQUIRE(dev);
    auto* bd = dynamic_cast<background_device*>(dev->getDevice());
    REQUIRE(bd);
    pump_for(100);
    initial = device_sizes(view, *dev);

    // A picture rendered for another ratio than the view's
    bd->node()->setSize({17, 13});
    staleEvent = bd->node()->size();
    QEvent ev{QEvent::DevicePixelRatioChange};
    QCoreApplication::sendEvent(&gv, &ev);
    pump_for(100);
    afterEvent = device_sizes(view, *dev);

    auto* w = gv.window() ? gv.window()->windowHandle() : nullptr;
    REQUIRE(w);
    bd->node()->setSize({19, 11});
    staleSignal = bd->node()->size();
    Q_EMIT w->screenChanged(w->screen());
    pump_for(100);
    afterSignal = device_sizes(view, *dev);

    ctx.docManager.forceCloseDocument(ctx, *doc);
    pump_for(200);
  });

  if(skipped)
    SKIP("the document has no scenario view to draw a background in");

  CAPTURE(initial, staleEvent, afterEvent, staleSignal, afterSignal);
  CHECK(initial.node == initial.pixels);
  CHECK(afterEvent.view == initial.view);
  CHECK(afterEvent.node == afterEvent.pixels);
  CHECK(afterEvent.renderTarget == afterEvent.pixels);
  CHECK(afterSignal.view == initial.view);
  CHECK(afterSignal.node == afterSignal.pixels);
  CHECK(afterSignal.renderTarget == afterSignal.pixels);
}

TEST_CASE(
    "child: a Background window device on a window moved to a denser screen",
    "[.][dpr-move-child]")
{
  bool skipped{};
  std::string skipReason;
  DeviceSizes before{}, after{};
  qreal dprBefore{}, dprAfter{};
  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    QScreen* low{};
    QScreen* high{};
    for(auto* s : QGuiApplication::screens())
    {
      if(s->devicePixelRatio() == 1.)
        low = s;
      else if(s->devicePixelRatio() == 2.)
        high = s;
    }
    if(!low || !high)
    {
      skipped = true;
      skipReason = "no pair of screens at device pixel ratios 1 and 2";
      return;
    }

    auto* doc = score::test::new_document(ctx);
    if(!doc || !scenario_view(*doc))
    {
      skipped = true;
      skipReason = "no document with a scenario view";
      return;
    }
    auto& view = *scenario_view(*doc);
    auto* top = view.view().window();
    REQUIRE(top);
    // Small enough for either screen
    top->resize(440, 420);
    top->move(low->geometry().topLeft() + QPoint{20, 20});
    top->show();
    pump_for(300);
    REQUIRE(top->windowHandle());
    REQUIRE(top->windowHandle()->screen() == low);

    auto* dev = add_background_device(doc->context(), QStringLiteral("Win"));
    REQUIRE(dev);
    pump_for(100);
    before = device_sizes(view, *dev);
    dprBefore = view.view().devicePixelRatioF();

    top->move(high->geometry().topLeft() + QPoint{20, 20});
    QElapsedTimer t;
    t.start();
    while(top->windowHandle()->screen() != high && t.elapsed() < 3000)
      pump_for(20);
    pump_for(300);
    REQUIRE(top->windowHandle()->screen() == high);
    after = device_sizes(view, *dev);
    dprAfter = view.view().devicePixelRatioF();

    ctx.docManager.forceCloseDocument(ctx, *doc);
    pump_for(200);
  });

  if(skipped)
  {
    say(kMoveSkip, skipReason);
    return;
  }
  CAPTURE(before, after, dprBefore, dprAfter);
  REQUIRE(dprBefore == 1.);
  REQUIRE(dprAfter == 2.);
  CHECK(before.node == before.pixels);
  CHECK(after.node == after.pixels);
  CHECK(after.renderTarget == after.pixels);
  say(kMoveDone, "view " + to_string(before.view) + " -> " + to_string(after.view));
}

TEST_CASE(
    "a Background window device follows its window to a screen of another density",
    "[gfx][window][device][background][dpr]")
{
  if(QGuiApplication::platformName() != QStringLiteral("xcb")
     && !qEnvironmentVariableIsSet("DISPLAY"))
    SKIP("needs an X server to split into two monitors");
  const QString self = QFileInfo{QStringLiteral("/proc/self/exe")}.canonicalFilePath();
  if(self.isEmpty())
    SKIP("cannot start this test binary again: /proc/self/exe is unavailable");

  const auto xrandr = [](const QStringList& args) {
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(QStringLiteral("xrandr"), args);
    if(!p.waitForFinished(10000) || p.exitStatus() != QProcess::NormalExit)
      return false;
    return p.exitCode() == 0;
  };
  // Two monitors side by side on the X screen of the private Xvfb the tests
  // run on, whose only output is named "screen"
  if(!xrandr({"--setmonitor", "DPR1", "960/254x1080/286+0+0", "screen"}))
    SKIP("xrandr cannot split this X screen into monitors");
  const auto restore = qScopeGuard([&] {
    xrandr({"--delmonitor", "DPR2"});
    xrandr({"--delmonitor", "DPR1"});
  });
  if(!xrandr({"--setmonitor", "DPR2", "960/254x1080/286+960+0", "none"}))
    SKIP("xrandr cannot add a second monitor to this X screen");

  auto env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
  env.insert(QStringLiteral("QT_SCREEN_SCALE_FACTORS"), QStringLiteral("DPR1=1;DPR2=2"));
  env.remove(QStringLiteral("QT_SCALE_FACTOR"));
  env.remove(QStringLiteral("SCORE_TEST_EXPECT_DPR"));

  QProcess child;
  child.setProcessEnvironment(env);
  child.setProcessChannelMode(QProcess::MergedChannels);
  child.start(self, {QStringLiteral("[dpr-move-child]")});
  REQUIRE(child.waitForStarted(30000));
  const bool finished = child.waitForFinished(300000);
  if(!finished)
    child.kill();
  const auto log = QString::fromUtf8(child.readAll());
  INFO(log.toStdString());
  REQUIRE(finished);
  REQUIRE(child.exitStatus() == QProcess::NormalExit);
  if(log.contains(QString::fromLatin1(kMoveSkip)))
    SKIP("the child could not set up two screens: see its output");
  CHECK(child.exitCode() == 0);
  CHECK(log.contains(QString::fromLatin1(kMoveDone)));
}
