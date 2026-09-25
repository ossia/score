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

#include <QTcpServer>
#include <QWebSocket>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>
#include <score_test/Scriptable.hpp>

#include <catch2/catch_all.hpp>

namespace
{
using score::test::wait_until;
const QString smooth_uuid = QStringLiteral("bf603921-5a48-4aa5-9bc1-48a762be6467");

// A port nothing listens on right now. The Receiver does not report the port it
// bound, so it cannot be handed 0; the local device already takes
// SCORE_LOCAL_WS_PORT.
quint16 free_port()
{
  QTcpServer probe;
  REQUIRE(probe.listen(QHostAddress::LocalHost, 0));
  return probe.serverPort();
}

QUrl url(quint16 port)
{
  return QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port));
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
    const auto port = free_port();
    RemoteControl::WS::Receiver receiver{dctx, port};
    list.setLocalDevice(local);

    QStringList received;
    QWebSocket client;
    QObject::connect(&client, &QWebSocket::textMessageReceived, &client, [&](const QString& m) {
      received.push_back(m);
    });
    client.open(url(port));
    REQUIRE(wait_until([&] { return client.state() == QAbstractSocket::ConnectedState; }));

    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(score::test::control_named(*proc, QStringLiteral("Amount")), true);
    REQUIRE(wait_until([&] {
      return ossia::any_of(received, [](auto& m) {
        return m.contains("\"Scriptable\"") && m.contains("\"Name\":\"fx\"");
      });
    }));

    received.clear();
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("reverb"));
    REQUIRE(wait_until([&] {
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
    const auto port = free_port();
    RemoteControl::WS::Receiver receiver{dctx, port};

    QStringList received;
    QWebSocket client;
    QObject::connect(&client, &QWebSocket::textMessageReceived, &client, [&](const QString& m) {
      received.push_back(m);
    });
    client.open(url(port));
    REQUIRE(wait_until([&] { return client.state() == QAbstractSocket::ConnectedState; }));

    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetProcessScriptable>(*proc, true);
    REQUIRE(wait_until([&] {
      return ossia::any_of(received, [](auto& m) { return m.contains("\"Name\":\"fx\""); });
    }));

    auto& plug = dctx.plugin<Execution::DocumentPlugin>();
    auto run = [&] { score::test::run_exec(plug); };
    plug.reload(true, score::test::base_interval(*doc));
    run();

    auto& itv = score::test::base_interval(*doc);
    disp.submit(new Scenario::Command::RemoveProcessFromInterval{itv, proc->id()});
    run();
    REQUIRE(wait_until([&] {
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
      late.open(url(port));
      REQUIRE(wait_until([&] {
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
    REQUIRE(wait_until([&] {
      return ossia::any_of(received, [](auto& m) {
        return m.contains("\"Scriptable\"") && m.contains("\"Name\":\"fx\"");
      });
    }));
    CHECK(!ossia::any_of(received, [](auto& m) { return m.contains("(retired)"); }));
    client.close();
    plug.clear();
    score::test::process_events();
  });
}
