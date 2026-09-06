// The remote-control WebSocket API is a way in to the machine score runs on:
// it sets device parameters, drives transport, and -- if allowed -- evaluates
// JavaScript. These pin the controls on who may do that.

#include <RemoteControl/Settings/Model.hpp>
#include <RemoteControl/Websockets/DocumentPlugin.hpp>

#include <score/application/ApplicationContext.hpp>

#include <QElapsedTimer>
#include <QtWebSockets/QWebSocket>

#include <QAbstractSocket>

#include <JS/ConsolePanel.hpp>

#include <QJSEngine>
#include <QJSValue>
#include <score/serialization/JSONVisitor.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_all.hpp>

namespace
{
//! Run the event loop until `pred` holds or we give up.
template <typename Pred>
bool spin_until(Pred pred, int timeoutMs = 3000)
{
  QElapsedTimer t;
  t.start();
  while(!pred())
  {
    if(t.elapsed() > timeoutMs)
      return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }
  return true;
}

RemoteControl::WS::ReceiverSettings testSettings(const QString& token)
{
  RemoteControl::WS::ReceiverSettings s;
  s.port = 0; // let the OS pick, so parallel test runs do not collide
  s.address = QStringLiteral("127.0.0.1");
  s.token = token;
  s.allowScripting = false;
  return s;
}
}

TEST_CASE("The remote control server refuses a client with no token", "[remote]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    RemoteControl::WS::Receiver receiver{doc->context()};
    receiver.open(testSettings(QStringLiteral("the-right-token")));
    REQUIRE(receiver.isOpen());

    QWebSocket client;
    bool connected{};
    QObject::connect(&client, &QWebSocket::connected, [&] { connected = true; });

    client.open(QUrl{QStringLiteral("ws://127.0.0.1:%1/").arg(receiver.port())});

    // The socket may reach "connected" at the protocol level before the server
    // hangs up, so what matters is that it does not stay a client of ours.
    spin_until([&] { return !receiver.clients().empty(); }, 1000);
    CHECK(receiver.clients().empty());
  });
}

TEST_CASE("The remote control server refuses a client with the wrong token", "[remote]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    RemoteControl::WS::Receiver receiver{doc->context()};
    receiver.open(testSettings(QStringLiteral("the-right-token")));
    REQUIRE(receiver.isOpen());

    QWebSocket client;
    client.open(QUrl{QStringLiteral("ws://127.0.0.1:%1/?token=guess")
                         .arg(receiver.port())});

    spin_until([&] { return !receiver.clients().empty(); }, 1000);
    CHECK(receiver.clients().empty());
  });
}

TEST_CASE("The remote control server serves a client with the token", "[remote]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    RemoteControl::WS::Receiver receiver{doc->context()};
    receiver.open(testSettings(QStringLiteral("the-right-token")));
    REQUIRE(receiver.isOpen());

    QWebSocket client;
    QStringList received;
    QObject::connect(
        &client, &QWebSocket::textMessageReceived,
        [&](const QString& m) { received.push_back(m); });

    client.open(QUrl{QStringLiteral("ws://127.0.0.1:%1/?token=the-right-token")
                         .arg(receiver.port())});

    REQUIRE(spin_until([&] { return !receiver.clients().empty(); }));

    // On accepting a client the server sends it the device tree.
    REQUIRE(spin_until([&] { return !received.empty(); }));
    CHECK(received.front().contains("DeviceTree"));
  });
}

TEST_CASE("An empty token serves nobody", "[remote]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    // A blank token must not degrade to "no password required": the settings
    // generate one precisely so that this state is unreachable, and if it is
    // reached anyway the server has to stay shut rather than open wide.
    RemoteControl::WS::Receiver receiver{doc->context()};
    receiver.open(testSettings(QString{}));
    REQUIRE(receiver.isOpen());

    QWebSocket client;
    client.open(QUrl{QStringLiteral("ws://127.0.0.1:%1/").arg(receiver.port())});

    spin_until([&] { return !receiver.clients().empty(); }, 1000);
    CHECK(receiver.clients().empty());
  });
}

TEST_CASE("Scripting is off unless asked for", "[remote]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    // The Console message evaluates arbitrary JavaScript in this process. It
    // used to be served to anyone who could reach the port.
    auto& settings = ctx.settings<RemoteControl::Settings::Model>();
    CHECK_FALSE(settings.getAllowScripting());
    CHECK_FALSE(settings.getEnabled());
    CHECK_FALSE(settings.getToken().isEmpty());
  });
}

namespace
{
//! Ask the server to run some JavaScript, over an accepted connection.
void sendConsole(
    RemoteControl::WS::Receiver& receiver, QWebSocket& client, const QString& code)
{
  rapidjson::StringBuffer buf;
  JsonWriter w{buf};
  w.StartObject();
  w.Key("Message");
  w.String("Console");
  w.Key("Code");
  const auto utf8 = code.toUtf8();
  w.String(utf8.constData(), utf8.size());
  w.EndObject();

  client.sendTextMessage(QString::fromUtf8(buf.GetString(), buf.GetLength()));
}

JS::PanelDelegate& console(const score::GUIApplicationContext& ctx)
{
  auto* p = ctx.findPanel<JS::PanelDelegate>();
  SCORE_ASSERT(p);
  return *p;
}

int probeValue(const score::GUIApplicationContext& ctx)
{
  return console(ctx)
      .engine()
      .globalObject()
      .property(QStringLiteral("__remoteProbe"))
      .toInt();
}
}

TEST_CASE("Scripting refused is scripting not run", "[remote]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    // The settings default was all that was ever asserted, and a default is not
    // an enforcement: deleting the guard in the Console handler left every one
    // of these tests passing.
    auto settings = testSettings(QStringLiteral("tok"));
    settings.allowScripting = false;

    RemoteControl::WS::Receiver receiver{doc->context()};
    receiver.open(settings);
    REQUIRE(receiver.isOpen());

    QWebSocket client;
    client.open(QUrl{
        QStringLiteral("ws://127.0.0.1:%1/?token=tok").arg(receiver.port())});
    REQUIRE(spin_until([&] { return !receiver.clients().empty(); }));

    // Both ends: the server accepting is not the client being ready to send,
    // and sendTextMessage on a socket that is not open yet is dropped -- which
    // would make a refusal indistinguishable from a message never sent.
    REQUIRE(spin_until(
        [&] { return client.state() == QAbstractSocket::ConnectedState; }));

    console(ctx).engine().evaluate(QStringLiteral("__remoteProbe = 0"));
    REQUIRE(probeValue(ctx) == 0);

    sendConsole(receiver, client, QStringLiteral("__remoteProbe = 1"));

    // Nothing to wait for when it works, so give it time to fail: a refusal
    // that only looks like one because the message had not arrived yet would
    // pass whatever the guard did.
    spin_until([&] { return probeValue(ctx) != 0; }, 1500);
    CHECK(probeValue(ctx) == 0);
  });
}

TEST_CASE("Scripting allowed is scripting run", "[remote]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    // The other half: with the setting on, the same message does evaluate --
    // otherwise the test above would pass against a Console handler that was
    // simply broken, and the feature would be silently dead.
    auto settings = testSettings(QStringLiteral("tok"));
    settings.allowScripting = true;

    RemoteControl::WS::Receiver receiver{doc->context()};
    receiver.open(settings);
    REQUIRE(receiver.isOpen());

    QWebSocket client;
    client.open(QUrl{
        QStringLiteral("ws://127.0.0.1:%1/?token=tok").arg(receiver.port())});
    REQUIRE(spin_until([&] { return !receiver.clients().empty(); }));

    // Both ends: the server accepting is not the client being ready to send,
    // and sendTextMessage on a socket that is not open yet is dropped -- which
    // would make a refusal indistinguishable from a message never sent.
    REQUIRE(spin_until(
        [&] { return client.state() == QAbstractSocket::ConnectedState; }));

    console(ctx).engine().evaluate(QStringLiteral("__remoteProbe = 0"));
    REQUIRE(probeValue(ctx) == 0);

    sendConsole(receiver, client, QStringLiteral("__remoteProbe = 7"));
    REQUIRE(spin_until([&] { return probeValue(ctx) == 7; }));
  });
}
