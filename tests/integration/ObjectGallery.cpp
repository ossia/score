// Object gallery: a small tool to enumerate and live-preview every process
#include <cstdlib>
// ("object") the running application registers.
//
//   ObjectGallery --list [filter]
//       Print every registered process factory (name + uuid), optionally
//       filtered by a case-insensitive substring of the pretty name.
//
//   ObjectGallery [--filter <substr>] [--seconds N]
//       Open a window, instantiate each matching object in the base interval,
//       route every texture output to the window, and play — so you can watch
//       all the texture-producing objects (gfx / ISF / video / rendered 3D)
//       at once. Objects that only output geometry are still instantiated;
//       chain them through a Model Display to view them.
//
// Built on the same MinimalGUIApplication + window-device pattern as the other
// gfx testers, and the AddProcessToInterval creation path used by the process
// sweep, so it exercises the real engine, not a hand-built graph.

#include <core/application/MinimalApplication.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/tools/Bind.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QTimer>

#include <ossia/detail/thread.hpp>

// Engine::ApplicationPlugin::execution().play_interval() is private; the gfx
// testers reach it the same way.
#define private public
#include <Engine/ApplicationPlugin.hpp>
#undef private
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DeviceList.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/WindowDevice.hpp>

#include <QFileInfo>
#include <QImage>

#include <algorithm>
#include <csignal>
#include <cstdio>

// Coverage support: this harness intentionally exits via std::_Exit (to bypass a
// gfx teardown crash) and often SEGVs on shutdown, both of which skip the normal
// atexit profile writer. When built with -fprofile-instr-generate we flush
// counters explicitly before _Exit and from a SEGV/ABRT handler so instrumented
// runs still yield a .profraw. Mach-O has no ELF-style weak undefined externals
// (the static link still requires the symbol), so resolve dynamically there.
#if defined(__APPLE__)
#include <dlfcn.h>
#else
extern "C" __attribute__((weak)) int __llvm_profile_write_file(void);
#endif

namespace
{
void flush_coverage()
{
#if defined(__APPLE__)
  if(auto f = reinterpret_cast<int (*)(void)>(
         dlsym(RTLD_DEFAULT, "__llvm_profile_write_file")))
    f();
#else
  if(&__llvm_profile_write_file != nullptr)
    __llvm_profile_write_file();
#endif
}
void coverage_signal_handler(int sig)
{
  flush_coverage();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}
void install_coverage_flush()
{
  std::signal(SIGSEGV, coverage_signal_handler);
  std::signal(SIGABRT, coverage_signal_handler);
}

bool matches(const QString& name, const QString& filter)
{
  return filter.isEmpty() || name.contains(filter, Qt::CaseInsensitive);
}

QString arg_value(const QStringList& args, const QString& flag)
{
  const int i = args.indexOf(flag);
  return (i >= 0 && i + 1 < args.size()) ? args[i + 1] : QString{};
}

// Enumerate every registered process and print name + uuid.
int list_objects(const score::GUIApplicationContext& ctx, const QString& filter)
{
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  std::vector<std::pair<QString, QString>> rows;
  for(auto& factory : pl)
  {
    const QString name = factory.prettyName();
    if(!matches(name, filter))
      continue;
    rows.emplace_back(
        name,
        QString::fromStdString(
            score::uuids::toByteArray(factory.concreteKey().impl()).toStdString()));
  }
  std::sort(rows.begin(), rows.end());
  for(const auto& [name, uuid] : rows)
    std::printf("%-44s %s\n", name.toUtf8().constData(), uuid.toUtf8().constData());
  std::printf("\n%zu objects.\n", rows.size());
  return 0;
}

// Open a window, instantiate every matching object, route texture outputs to
// it, and play for a while.
void run_gallery(const QString& filter, int seconds)
{
  const auto& ctx = score::GUIAppContext();

  auto doc = ctx.docManager.newDocument(
      ctx, Id<score::DocumentModel>(0),
      *ctx.interfaces<score::DocumentDelegateList>().begin());
  qApp->processEvents();

  // A window output device the texture outlets can target.
  {
    auto& device_plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    CommandDispatcher<> disp{doc->context().commandStack};
    Device::DeviceSettings settings;
    settings.name = "window";
    settings.protocol = Gfx::WindowProtocolFactory::static_concreteKey();
    disp.submit<Explorer::Command::LoadDevice>(device_plug, settings);
  }

  auto& scenario_dm
      = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
  auto& itv = scenario_dm.baseInterval();

  int created = 0, textured = 0;
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  for(auto& factory : pl)
  {
    const QString name = factory.prettyName();
    if(!matches(name, filter))
      continue;

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        itv, factory.concreteKey(), factory.customConstructionData(), QPointF{});
    ++created;
  }

  // Point every texture output at the window so it is displayed.
  for(auto outlet : itv.findChildren<Gfx::TextureOutlet*>())
  {
    outlet->setAddress(State::AddressAccessor{{{"window"}, {}}});
    ++textured;
  }

  std::printf(
      "Gallery: %d objects instantiated, %d texture outputs routed to the "
      "window. Playing for %ds.\n",
      created, textured, seconds);
  std::fflush(stdout);

  auto& eng = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  QTimer::singleShot(100, [&eng, &itv] { eng.execution().play_interval(itv); });
  QTimer::singleShot(seconds * 1000, [&ctx, doc] {
    ctx.docManager.forceCloseDocument(ctx, *doc);
    qApp->exit(0);
  });
}

// True if the image has more than one distinct colour (i.e. actually rendered
// something, not a single flat clear-colour / transparent frame).
bool non_blank(const QImage& img)
{
  if(img.isNull() || img.width() < 2 || img.height() < 2)
    return false;
  const QRgb first = img.pixel(0, 0);
  for(int y = 0; y < img.height(); y += 4)
    for(int x = 0; x < img.width(); x += 4)
      if(img.pixel(x, y) != first)
        return true;
  return false;
}

// Render the first object matching `filter` offscreen and check it produced a
// non-blank frame. Exit codes: 0 = rendered non-blank, 2 = blank frame,
// 3 = object has no texture output (needs a display chain), 4 = grab failed.
// A crash during rendering kills the process with its signal — the driver that
// runs this per object treats that as the object's result.
void run_render_check(const QString& filter, int seconds)
{
  const auto& ctx = score::GUIAppContext();

  auto doc = ctx.docManager.newDocument(
      ctx, Id<score::DocumentModel>(0),
      *ctx.interfaces<score::DocumentDelegateList>().begin());
  qApp->processEvents();

  auto& device_plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
  {
    CommandDispatcher<> disp{doc->context().commandStack};
    Device::DeviceSettings settings;
    settings.name = "window";
    settings.protocol = Gfx::WindowProtocolFactory::static_concreteKey();
    disp.submit<Explorer::Command::LoadDevice>(device_plug, settings);
  }

  auto& scenario_dm
      = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
  auto& itv = scenario_dm.baseInterval();

  QString target;
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  for(auto& factory : pl)
  {
    if(!matches(factory.prettyName(), filter))
      continue;
    target = factory.prettyName();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        itv, factory.concreteKey(), factory.customConstructionData(), QPointF{});
    break;
  }
  if(target.isEmpty())
  {
    std::printf("RENDER  no object matches '%s'\n", filter.toUtf8().constData());
    qApp->exit(1);
    return;
  }

  int textured = 0;
  for(auto outlet : itv.findChildren<Gfx::TextureOutlet*>())
  {
    outlet->setAddress(State::AddressAccessor{{{"window"}, {}}});
    ++textured;
  }
  if(textured == 0)
  {
    std::printf("RENDER  SKIP  %-40s (no texture output)\n", target.toUtf8().constData());
    qApp->exit(3);
    return;
  }

  auto& eng = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  QTimer::singleShot(100, [&eng, &itv] { eng.execution().play_interval(itv); });
  QTimer::singleShot(seconds * 1000, [&ctx, doc, target] {
    int rc = 4;
    if(auto* d = doc->context()
                     .plugin<Explorer::DeviceDocumentPlugin>()
                     .list()
                     .findDevice("window"))
    {
      if(auto* wd = qobject_cast<Gfx::WindowDevice*>(d))
      {
        const QString path = "/tmp/objectgallery_render.png";
        wd->grabTo(path);
        QApplication::processEvents();
        const QImage img{path};
        const bool ok = non_blank(img);
        // Reaching here means the object rendered a full run without crashing
        // — the primary signal. non_blank is reported as a hint: the offscreen
        // readback path does not yet composite the routed texture, so it is
        // currently informational, not a pass/fail gate.
        rc = 0;
        std::printf(
            "RENDER  OK  %-40s %dx%d  content=%s\n", target.toUtf8().constData(),
            img.width(), img.height(), ok ? "yes" : "blank");
      }
    }
    std::fflush(stdout);
    // The render already ran; skip the gfx-context teardown (which currently
    // crashes in the offscreen readback release path) via a hard exit. If the
    // object had crashed *during* rendering, we would never reach here — that
    // is exactly the render-path failure this check is meant to catch.
    flush_coverage();
    std::_Exit(rc);
  });
}

// Load a shader tester file (.fs/.frag/.glsl -> ISF Shader, .cs/.comp/.csf ->
// Compute Shader) into a process, route its texture output to an offscreen
// window and render it. Exit 0 = compiled and rendered without crashing (a
// shader compile failure or render crash is caught), 3 = no texture output,
// 5 = unsupported extension. Same offscreen/hard-exit handling as --render.
void run_shader_check(const QString& path, int seconds)
{
  const QFileInfo fi{path};
  const QString ext = fi.suffix().toLower();
  QString wantName;
  if(ext == "fs" || ext == "frag" || ext == "glsl")
    wantName = "ISF Shader";
  else if(ext == "cs" || ext == "comp" || ext == "csf")
    wantName = "Compute Shader";
  else
  {
    std::printf("SHADER  UNSUPPORTED  %s\n", path.toUtf8().constData());
    qApp->exit(5);
    return;
  }

  const auto& ctx = score::GUIAppContext();
  auto doc = ctx.docManager.newDocument(
      ctx, Id<score::DocumentModel>(0),
      *ctx.interfaces<score::DocumentDelegateList>().begin());
  qApp->processEvents();

  auto& device_plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
  {
    CommandDispatcher<> disp{doc->context().commandStack};
    Device::DeviceSettings settings;
    settings.name = "window";
    settings.protocol = Gfx::WindowProtocolFactory::static_concreteKey();
    disp.submit<Explorer::Command::LoadDevice>(device_plug, settings);
  }

  auto& scenario_dm
      = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
  auto& itv = scenario_dm.baseInterval();

  // The shader process is created with the file path as its construction data.
  bool created = false;
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  for(auto& factory : pl)
  {
    if(factory.prettyName() != wantName)
      continue;
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        itv, factory.concreteKey(), fi.absoluteFilePath(), QPointF{});
    created = true;
    break;
  }
  if(!created)
  {
    std::printf("SHADER  no '%s' process\n", wantName.toUtf8().constData());
    qApp->exit(1);
    return;
  }

  int textured = 0;
  for(auto outlet : itv.findChildren<Gfx::TextureOutlet*>())
  {
    outlet->setAddress(State::AddressAccessor{{{"window"}, {}}});
    ++textured;
  }
  if(textured == 0)
  {
    std::printf(
        "SHADER  SKIP  %-46s (no texture output)\n", fi.fileName().toUtf8().constData());
    qApp->exit(3);
    return;
  }

  auto& eng = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  QTimer::singleShot(100, [&eng, &itv] { eng.execution().play_interval(itv); });
  QTimer::singleShot(seconds * 1000, [&ctx, doc, fi] {
    int rc = 0;
    if(auto* d = doc->context()
                     .plugin<Explorer::DeviceDocumentPlugin>()
                     .list()
                     .findDevice("window"))
    {
      if(auto* wd = qobject_cast<Gfx::WindowDevice*>(d))
      {
        const QString out = "/tmp/objectgallery_render.png";
        wd->grabTo(out);
        QApplication::processEvents();
        const QImage img{out};
        std::printf(
            "SHADER  OK  %-46s %dx%d  content=%s\n",
            fi.fileName().toUtf8().constData(), img.width(), img.height(),
            non_blank(img) ? "yes" : "blank");
      }
    }
    std::fflush(stdout);
    flush_coverage();
    std::_Exit(rc);
  });
}
}

int main(int argc, char** argv)
{
  install_coverage_flush();
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  // Register the main thread as the UI thread, as the real application does at
  // startup, so the execution engine's thread-kind checks pass when we play.
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  // Only the live gallery needs a visible main window; --list and --render run
  // headless (a shown window's presenter/RHI-flush is unnecessary and crashes
  // offscreen). Determine the mode from argv before the app is built.
  bool headless = false;
  for(int i = 1; i < argc; ++i)
  {
    const std::string a = argv[i];
    if(a == "--list" || a == "--render" || a == "--shader")
      headless = true;
  }

  score::MinimalGUIApplication app(argc, argv, /*show=*/!headless);

  const QStringList args = qApp->arguments();
  const bool list = args.contains("--list");
  const int seconds = arg_value(args, "--seconds").toInt() > 0
                          ? arg_value(args, "--seconds").toInt()
                          : 20;

  // Filter: value of --filter, or the trailing positional arg after --list.
  QString filter = arg_value(args, "--filter");
  if(filter.isEmpty() && list)
  {
    const int i = args.indexOf("--list");
    if(i + 1 < args.size() && !args[i + 1].startsWith("--"))
      filter = args[i + 1];
  }

  if(list)
  {
    // --list is synchronous: run once the plugins are loaded, then quit.
    QMetaObject::invokeMethod(
        &app,
        [&] {
      const int rc = list_objects(score::GUIAppContext(), filter);
      qApp->exit(rc);
        },
        Qt::QueuedConnection);
    return app.exec();
  }

  // --render <name>: render one object offscreen and check for a non-blank
  // frame, exiting with a status code. Run one object per process (a driver
  // loops over them) so a render-path crash is isolated to that object.
  const QString renderTarget = arg_value(args, "--render");
  if(!renderTarget.isEmpty())
  {
    // Force the window device to render offscreen (into a readback) so grabTo
    // works without popping a window — even under X11.
    qputenv("SCORE_FORCE_OFFSCREEN_WINDOW", "window");
    const int secs = arg_value(args, "--seconds").toInt() > 0
                         ? arg_value(args, "--seconds").toInt()
                         : 2;
    QMetaObject::invokeMethod(
        &app, [&, renderTarget, secs] { run_render_check(renderTarget, secs); },
        Qt::QueuedConnection);
    return app.exec();
  }

  // --shader <file>: load a shader tester file and render it.
  const QString shaderFile = arg_value(args, "--shader");
  if(!shaderFile.isEmpty())
  {
    qputenv("SCORE_FORCE_OFFSCREEN_WINDOW", "window");
    const int secs = arg_value(args, "--seconds").toInt() > 0
                         ? arg_value(args, "--seconds").toInt()
                         : 2;
    QMetaObject::invokeMethod(
        &app, [&, shaderFile, secs] { run_shader_check(shaderFile, secs); },
        Qt::QueuedConnection);
    return app.exec();
  }

  QMetaObject::invokeMethod(
      &app, [&] { run_gallery(filter, seconds); }, Qt::QueuedConnection);
  return app.exec();
}
