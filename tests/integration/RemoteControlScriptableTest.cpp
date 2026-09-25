// Clients are notified of renames regardless of when the local device was set.

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Explorer/DeviceList.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <RemoteControl/Websockets/DocumentPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QWebSocket>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_all.hpp>

namespace
{
const QString smooth_uuid = QStringLiteral("bf603921-5a48-4aa5-9bc1-48a762be6467");
constexpr quint16 port = 10391;

template <typename F>
bool waitFor(F&& condition, int ms = 3000)
{
  QElapsedTimer t;
  t.start();
  while(!condition() && t.elapsed() < ms)
  {
    QCoreApplication::sendPostedEvents();
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  }
  return condition();
}

Process::ControlInlet& control(Process::ProcessModel& p, const QString& name)
{
  for(auto inlet : p.inlets())
    if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet); ctl && ctl->name() == name)
      return *ctl;
  FAIL("no control named " << name.toStdString());
  throw;
}
}

TEST_CASE(
    "a remote client hears of renames when the local device was set after the server",
    "[integration][remotecontrol][scriptable]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    auto& list = dctx.plugin<Explorer::DeviceDocumentPlugin>().list();
    auto local = list.localDevice();
    REQUIRE(local);

    // Create the server before setting the local device
    list.setLocalDevice(nullptr);
    RemoteControl::WS::Receiver receiver{dctx, port};
    list.setLocalDevice(local);

    QStringList received;
    QWebSocket client;
    QObject::connect(&client, &QWebSocket::textMessageReceived, &client, [&](const QString& m) {
      received.push_back(m);
    });
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    REQUIRE(waitFor([&] { return client.state() == QAbstractSocket::ConnectedState; }));

    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(control(*proc, QStringLiteral("Amount")), true);
    REQUIRE(waitFor([&] {
      return ossia::any_of(received, [](auto& m) {
        return m.contains("\"Scriptable\"") && m.contains("\"Name\":\"fx\"");
      });
    }));

    received.clear();
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("reverb"));
    REQUIRE(waitFor([&] {
      return ossia::any_of(received, [](auto& m) {
        return m.contains("ScriptableRenamed") && m.contains("score:/controls/reverb");
      });
    }));
    client.close();
  });
}

TEST_CASE(
    "a remote client hears of a process restored while playing",
    "[integration][remotecontrol][scriptable]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    RemoteControl::WS::Receiver receiver{dctx, port + 1};

    QStringList received;
    QWebSocket client;
    QObject::connect(&client, &QWebSocket::textMessageReceived, &client, [&](const QString& m) {
      received.push_back(m);
    });
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port + 1)));
    REQUIRE(waitFor([&] { return client.state() == QAbstractSocket::ConnectedState; }));

    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetProcessScriptable>(*proc, true);
    REQUIRE(waitFor([&] {
      return ossia::any_of(received, [](auto& m) { return m.contains("\"Name\":\"fx\""); });
    }));

    auto& plug = dctx.plugin<Execution::DocumentPlugin>();
    auto run = [&] {
      ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
      plug.runAllCommands();
      ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    };
    plug.reload(true, score::test::base_interval(*doc));
    run();

    auto& itv = score::test::base_interval(*doc);
    disp.submit(new Scenario::Command::RemoveProcessFromInterval{itv, proc->id()});
    run();
    REQUIRE(waitFor([&] {
      return ossia::any_of(received, [](auto& m) {
        return m.contains("ScriptableRemoved") && m.contains("score:/controls/fx");
      });
    }));

    // A client connecting now is not told of the node kept for an undo
    {
      QStringList first;
      QWebSocket late;
      QObject::connect(&late, &QWebSocket::textMessageReceived, &late, [&](const QString& m) {
        first.push_back(m);
      });
      late.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port + 1)));
      REQUIRE(waitFor([&] {
        return ossia::any_of(first, [](auto& m) { return m.contains("\"Scriptable\""); });
      }));
      auto msg = ossia::find_if(first, [](auto& m) { return m.contains("\"Scriptable\""); });
      CHECK(!msg->contains("\"Name\":\"fx"));
      CHECK(!msg->contains("(retired)"));
      late.close();
    }

    received.clear();
    doc->commandStack().undo();
    run();
    REQUIRE(waitFor([&] {
      return ossia::any_of(received, [](auto& m) {
        return m.contains("\"Scriptable\"") && m.contains("\"Name\":\"fx\"");
      });
    }));
    CHECK(!ossia::any_of(received, [](auto& m) { return m.contains("(retired)"); }));
    plug.clear();
    waitFor([] { return false; }, 100);
  });
}
