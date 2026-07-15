// Object gallery: a small tool to enumerate and live-preview every process
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
#include <Gfx/TexturePort.hpp>
#include <Gfx/WindowDevice.hpp>

#include <algorithm>
#include <cstdio>

namespace
{
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
}

int main(int argc, char** argv)
{
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  // Register the main thread as the UI thread, as the real application does at
  // startup, so the execution engine's thread-kind checks pass when we play.
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  score::MinimalGUIApplication app(argc, argv);

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

  QMetaObject::invokeMethod(
      &app, [&] { run_gallery(filter, seconds); }, Qt::QueuedConnection);
  return app.exec();
}
