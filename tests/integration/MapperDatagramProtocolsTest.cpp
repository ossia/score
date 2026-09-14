#include <score_test/Mapper.hpp>

#include <QFileInfo>
#include <QJsonArray>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkDatagram>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QUuid>
#include <QWebSocket>
#include <QWebSocketServer>

#include <libremidi/libremidi.hpp>

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#if defined(Q_OS_UNIX)
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

namespace
{
using score::test::mapper::fixture;

void substitute(QString& script, const QString& from, const QString& to, int count = 1)
{
  INFO("endpoint substitution: " << from.toStdString());
  REQUIRE(script.count(from) == count);
  script.replace(from, to);
}

void substituteFirst(QString& script, const QString& from, const QString& to)
{
  const auto pos = script.indexOf(from);
  REQUIRE(pos >= 0);
  script.replace(pos, from.size(), to);
}

void bindUdp(QUdpSocket& socket)
{
  REQUIRE(socket.bind(QHostAddress{QHostAddress::LocalHost}, quint16{0}));
}

QString port(int number) { return QStringLiteral("Port: %1").arg(number); }

void ready(fixture& f, const QString& name, const QString& path)
{
  REQUIRE(f.spin([&] { return f.contents(name).contains(name + ':' + path); }));
}

void value(fixture& f, const QString& name, const QString& path, const QVariant& expected)
{
  const bool received = f.spin([&] {
    return f.contents(name).value(name + ':' + path) == expected;
  });
  INFO("tree: " << QJsonDocument::fromVariant(f.contents(name)).toJson().toStdString());
  REQUIRE(received);
}

QNetworkDatagram receive(fixture& f, QUdpSocket& socket)
{
  REQUIRE(f.spin([&] { return socket.hasPendingDatagrams(); }));
  return socket.receiveDatagram();
}

void sendUdp(QUdpSocket& socket, quint16 target, const QByteArray& data)
{
  REQUIRE(socket.writeDatagram(data, QHostAddress::LocalHost, target) == data.size());
}

// Independent, literal OSC wire fixtures: padded address/type strings and
// network-order IEEE-754 values. No libossia encoder is used as the oracle.
QByteArray oscFloat(const QByteArray& address, const QByteArray& bits)
{
  QByteArray bytes = address;
  bytes.append('\0');
  while(bytes.size() % 4)
    bytes.append('\0');
  bytes.append(QByteArray::fromHex("2c660000"));
  bytes.append(QByteArray::fromHex(bits));
  return bytes;
}

#if defined(Q_OS_UNIX)
struct unix_datagram
{
  int fd{::socket(AF_UNIX, SOCK_DGRAM, 0)};
  QByteArray path;

  explicit unix_datagram(const QString& name) : path{name.toUtf8()}
  {
    REQUIRE(fd >= 0);
    const auto address = endpoint(path);
    REQUIRE(::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
  }
  ~unix_datagram()
  {
    if(fd >= 0)
      ::close(fd);
    ::unlink(path.constData());
  }
  static sockaddr_un endpoint(const QByteArray& path)
  {
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    REQUIRE(path.size() < qsizetype(sizeof(address.sun_path)));
    std::memcpy(address.sun_path, path.constData(), path.size());
    return address;
  }
  void send(const QString& target, const QByteArray& bytes)
  {
    const auto address = endpoint(target.toUtf8());
    REQUIRE(::sendto(fd, bytes.constData(), bytes.size(), 0,
                     reinterpret_cast<const sockaddr*>(&address), sizeof(address))
            == bytes.size());
  }
  QByteArray receive(fixture& f)
  {
    QByteArray bytes(65536, '\0');
    ssize_t size = -1;
    REQUIRE(f.spin([&] {
      size = ::recv(fd, bytes.data(), bytes.size(), MSG_DONTWAIT);
      if(size < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        FAIL("Unix datagram receive: " << std::strerror(errno));
      return size >= 0;
    }));
    bytes.resize(size);
    return bytes;
  }
};
#endif
}

TEST_CASE("Mapper UDP outbound sends strings and OSC", "[mapper][protocols][udp]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket peer;
    bindUdp(peer);
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_udp_outbound.qml");
    substitute(script, port(9001), port(peer.localPort()));
    f.createMapper("udp_out", script);
    REQUIRE(receive(f, peer).data() == "hello from udp");
    ready(f, "udp_out", "/send");
    f.push("udp_out", "/send", std::string{"sent through Mapper"});
    REQUIRE(receive(f, peer).data() == "sent through Mapper");
    f.push("udp_out", "/osc_send", 1.25f);
    REQUIRE(receive(f, peer).data() == oscFloat("/test/value", "3fa00000"));
    f.removeMapper("udp_out");
  });
}

TEST_CASE("Mapper UDP inbound converts datagrams to strings", "[mapper][protocols][udp]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket reservation, peer;
    bindUdp(reservation);
    bindUdp(peer);
    const auto target = reservation.localPort();
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_udp_inbound.qml");
    substitute(script, port(9001), port(target));
    reservation.close();
    f.createMapper("udp_in", script);
    ready(f, "udp_in", "/last_message");
    const QByteArray payload{"first\0second", 12};
    sendUdp(peer, target, payload);
    value(f, "udp_in", "/last_message", QString::fromUtf8(payload));
    sendUdp(peer, target, "next datagram");
    value(f, "udp_in", "/last_message", QString{"next datagram"});
    f.removeMapper("udp_in");
    REQUIRE(f.spin([&] {
      return reservation.bind(QHostAddress{QHostAddress::LocalHost}, target);
    }));
  });
}

TEST_CASE("Mapper UDP replies preserve each sender endpoint", "[mapper][protocols][udp]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket reservation, outbound, first, second;
    bindUdp(reservation);
    bindUdp(outbound);
    bindUdp(first);
    bindUdp(second);
    const auto target = reservation.localPort();
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_udp_reply.qml");
    REQUIRE(script.count(port(7000)) == 2);
    substituteFirst(script, port(7000), port(target));
    substituteFirst(script, port(7000), port(outbound.localPort()));
    reservation.close();
    f.createMapper("udp_reply", script);
    REQUIRE(receive(f, outbound).data() == "ping");
    ready(f, "udp_reply", "/server_received");
    f.push("udp_reply", "/send", std::string{"client command"});
    REQUIRE(receive(f, outbound).data() == "client command");
    for(auto* peer : {&first, &second})
    {
      const auto payload = QByteArray::number(peer->localPort());
      sendUdp(*peer, target, payload);
      const auto reply = receive(f, *peer);
      REQUIRE(reply.data() == "pong:" + payload);
      REQUIRE(reply.senderPort() == target);
      REQUIRE(reply.senderAddress() == QHostAddress{QHostAddress::LocalHost});
      value(f, "udp_reply", "/server_received", QString::fromUtf8(payload));
      value(f, "udp_reply", "/sender_host", QString{"127.0.0.1"});
      value(f, "udp_reply", "/sender_port", int(peer->localPort()));
    }
  });
}

TEST_CASE("Mapper UDP base64 encodes payloads and replies", "[mapper][protocols][udp]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket reservation, outbound, peer;
    bindUdp(reservation);
    bindUdp(outbound);
    bindUdp(peer);
    const auto target = reservation.localPort();
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_udp_base64.qml");
    REQUIRE(script.count(port(7010)) == 2);
    substituteFirst(script, port(7010), port(target));
    substituteFirst(script, port(7010), port(outbound.localPort()));
    reservation.close();
    f.createMapper("udp_base64", script);
    REQUIRE(receive(f, outbound).data() == QByteArray{"hello base64 over udp"}.toBase64());
    ready(f, "udp_base64", "/send");
    f.push("udp_base64", "/send", std::string{"encode me"});
    REQUIRE(receive(f, outbound).data() == "ZW5jb2RlIG1l");
    const QByteArray payload{"decode\0me", 9};
    sendUdp(peer, target, payload.toBase64());
    REQUIRE(receive(f, peer).data() == "YWNrOjE=");
    value(f, "udp_base64", "/last_message", QString::fromUtf8(payload));
    sendUdp(peer, target, QByteArray{"second"}.toBase64());
    REQUIRE(receive(f, peer).data() == "YWNrOjI=");
    value(f, "udp_base64", "/last_message", QString{"second"});
  });
}

TEST_CASE("Mapper OSC UDP encodes and parses independent wire packets", "[mapper][protocols][osc]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket reservation, peer;
    bindUdp(reservation);
    bindUdp(peer);
    const auto target = reservation.localPort();
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_osc_udp.qml");
    substitute(script, port(7000), port(target));
    substitute(script, port(7001), port(peer.localPort()));
    reservation.close();
    f.createMapper("osc_udp", script);
    ready(f, "osc_udp", "/send_float");
    f.push("osc_udp", "/send_float", 1.25f);
    REQUIRE(receive(f, peer).data() == oscFloat("/test/float", "3fa00000"));
    f.push("osc_udp", "/send_string", std::string{"hello"});
    // "/test/string" is 12 bytes, so its terminator plus OSC's 4-byte alignment
    // padding occupy 4 more: 16 bytes of address, then ",s\0\0", then
    // "hello\0\0\0".
    REQUIRE(receive(f, peer).data()
            == QByteArray::fromHex("2f746573742f737472696e67000000002c73000068656c6c6f000000"));
    sendUdp(peer, target, oscFloat("/peer/value", "c0200000"));
    value(f, "osc_udp", "/last_address", QString{"/peer/value"});
    value(f, "osc_udp", "/last_value", -2.5);
    sendUdp(peer, target, QByteArray::fromHex("2f706565722f696e740000002c6900000000002a"));
    value(f, "osc_udp", "/last_address", QString{"/peer/int"});
    value(f, "osc_udp", "/last_value", 42.0);
  });
}

TEST_CASE("Mapper WebSocket client exchanges text and binary and observes close", "[mapper][protocols][ws]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QWebSocketServer server{"Mapper peer", QWebSocketServer::NonSecureMode};
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    QStringList text;
    QList<QByteArray> binary;
    std::unique_ptr<QWebSocket> peer;
    QObject::connect(&server, &QWebSocketServer::newConnection, &server, [&] {
      peer.reset(server.nextPendingConnection());
      QObject::connect(peer.get(), &QWebSocket::textMessageReceived, &server,
                       [&](const QString& msg) { text.push_back(msg); });
      QObject::connect(peer.get(), &QWebSocket::binaryMessageReceived, &server,
                       [&](const QByteArray& msg) { binary.push_back(msg); });
    });
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_ws_outbound.qml");
    substitute(script, port(8080), port(server.serverPort()));
    f.createMapper("ws_out", script);
    REQUIRE(f.spin([&] { return !text.empty(); }));
    REQUIRE(text == QStringList{"hello from ws client"});
    value(f, "ws_out", "/status", QString{"connected"});
    f.push("ws_out", "/send_text", std::string{"text from Mapper"});
    REQUIRE(f.spin([&] { return text.size() == 2; }));
    REQUIRE(text.back() == "text from Mapper");
    const std::string bytes{"binary\0payload", 14};
    f.push("ws_out", "/send_binary", bytes);
    REQUIRE(f.spin([&] { return binary.size() == 1; }));
    REQUIRE(binary.front() == QByteArray(bytes.data(), bytes.size()));
    peer->sendTextMessage(QString::fromUtf8("peer text \xc3\xa9"));
    value(f, "ws_out", "/last_text", QString::fromUtf8("peer text \xc3\xa9"));
    const QByteArray incoming{"peer\0binary", 11};
    peer->sendBinaryMessage(incoming);
    value(f, "ws_out", "/last_binary", QString::fromUtf8(incoming));
    peer->close();
    value(f, "ws_out", "/status", QString{"disconnected"});
    f.removeMapper("ws_out");
  });
}

TEST_CASE("Mapper WebSocket server accepts real clients and releases its listener", "[mapper][protocols][ws]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QTcpServer reservation;
    REQUIRE(reservation.listen(QHostAddress::LocalHost, 0));
    const auto target = reservation.serverPort();
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_ws_inbound.qml");
    substitute(script, port(8080), port(target));
    reservation.close();
    f.createMapper("ws_in", script);
    ready(f, "ws_in", "/status");
    QWebSocket peer;
    peer.open(QUrl{QStringLiteral("ws://127.0.0.1:%1").arg(target)});
    REQUIRE(f.spin([&] { return peer.state() == QAbstractSocket::ConnectedState; }));
    value(f, "ws_in", "/status", QString{"client_connected"});
    // This fixture exposes connection state only, not per-connection messages.
    peer.close();
    REQUIRE(f.spin([&] { return peer.state() == QAbstractSocket::UnconnectedState; }));
    peer.open(QUrl{QStringLiteral("ws://127.0.0.1:%1").arg(target)});
    REQUIRE(f.spin([&] { return peer.state() == QAbstractSocket::ConnectedState; }));
    f.removeMapper("ws_in");
    REQUIRE(f.spin([&] { return peer.state() == QAbstractSocket::UnconnectedState; }));
    REQUIRE(f.spin([&] { return reservation.listen(QHostAddress::LocalHost, target); }));
  });
}

namespace
{
// The vendored inbound-WS fixture ignores the connection object it is handed, so
// the per-connection half of the server needs a script that actually uses it.
QString wsConnectionScript(quint16 target)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var client: null

  property var wsServer: Protocols.inboundWS({
    Transport: { Bind: "127.0.0.1", Port: %1 },
    onOpen: function(server) { Device.write("/status", "listening"); },
    onConnection: function(socket) {
      client = socket;
      socket.onTextMessage = function(msg) {
        Device.write("/last_text", msg);
        socket.write("echo:" + msg);
      };
      socket.onBinaryMessage = function(msg) {
        Device.write("/last_binary", msg.toString());
        socket.writeBinary(msg);
      };
      socket.onBytes = function(bytes) {
        Device.write("/last_bytes", bytes.toString());
      };
      socket.onClose = function() { Device.write("/status", "client_closed"); };
      Device.write("/status", "client_connected");
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "last_text", type: Ossia.Type.String, value: "" },
      { name: "last_binary", type: Ossia.Type.String, value: "" },
      { name: "last_bytes", type: Ossia.Type.String, value: "" },
      { name: "disconnect", type: Ossia.Type.Int,
        write: function(v) { if(client) client.close(); } }
    ];
  }
}
)qml")
      .arg(target);
}

// Records the order in which the script's handlers run, and how many times the
// close notification reached it. The socket's callbacks are QJSValues living on
// the Mapper thread, so none of them may be observed in the middle of another
// handler on that same thread, and a close is one event however it was caused.
QString wsClientScript(quint16 target, int busyMs)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var ws: null
  property string events: ""
  property int closes: 0

  function note(tag) {
    events = (events.length === 0) ? tag : (events + "," + tag);
    Device.write("/events", events);
  }

  function createTree() {
    ws = Protocols.outboundWS({
      Transport: { Host: "127.0.0.1", Port: %1 },
      onOpen: function(sock) { Device.write("/status", "connected"); },
      onTextMessage: function(msg) { note(msg); },
      onClose: function() {
        closes = closes + 1;
        Device.write("/close_count", closes);
        Device.write("/status", "disconnected");
      }
    });

    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "events", type: Ossia.Type.String, value: "" },
      { name: "close_count", type: Ossia.Type.Int, value: 0 },
      { name: "disconnect", type: Ossia.Type.Int,
        write: function(v) { ws.close(); } },
      { name: "busy", type: Ossia.Type.Int,
        write: function(v) {
          note("busy_start");
          var deadline = Date.now() + %2;
          while(Date.now() < deadline) {}
          note("busy_end");
        } }
    ];
  }
}
)qml")
      .arg(target)
      .arg(busyMs);
}

// A connection callback that shuts the whole server down: the connection object
// whose handler is running is owned by that server. onBytes is deliberately set
// too, because the raw-frame callback is read after the text one returned: it is
// the observable proof that the connection was still there.
QString wsReentrantCloseScript(quint16 target)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var wsServer: Protocols.inboundWS({
    Transport: { Bind: "127.0.0.1", Port: %1 },
    onClose: function() { Device.write("/status", "closed"); },
    onConnection: function(socket) {
      Device.write("/status", "client_connected");
      socket.onTextMessage = function(msg) {
        Device.write("/last_text", msg);
        if(msg === "close yourself")
          wsServer.close();
      };
      socket.onBytes = function(bytes) {
        Device.write("/last_bytes", bytes.toString());
      };
    }
  })

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String, value: "idle" },
      { name: "last_text", type: Ossia.Type.String, value: "" },
      { name: "last_bytes", type: Ossia.Type.String, value: "" }
    ];
  }
}
)qml")
      .arg(target);
}
}

TEST_CASE("Mapper WebSocket server exchanges messages with a client and closes it", "[mapper][protocols][ws]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QTcpServer reservation;
    REQUIRE(reservation.listen(QHostAddress::LocalHost, 0));
    const auto target = reservation.serverPort();
    reservation.close();
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("ws_conn", wsConnectionScript(target));
    // The server is created as a property initializer, before createTree(), so
    // its onOpen write lands nowhere: wait for the tree itself.
    ready(f, "ws_conn", "/status");

    QStringList text;
    QList<QByteArray> binary;
    bool disconnected = false;
    QWebSocket peer;
    QObject::connect(&peer, &QWebSocket::textMessageReceived, &peer,
                     [&](const QString& msg) { text.push_back(msg); });
    QObject::connect(&peer, &QWebSocket::binaryMessageReceived, &peer,
                     [&](const QByteArray& msg) { binary.push_back(msg); });
    QObject::connect(&peer, &QWebSocket::disconnected, &peer,
                     [&] { disconnected = true; });
    peer.open(QUrl{QStringLiteral("ws://127.0.0.1:%1").arg(target)});
    REQUIRE(f.spin([&] { return peer.state() == QAbstractSocket::ConnectedState; }));
    value(f, "ws_conn", "/status", QString{"client_connected"});

    peer.sendTextMessage(QStringLiteral("ping from client"));
    value(f, "ws_conn", "/last_text", QString{"ping from client"});
    // onBytes is the raw-frame callback: it sees text frames too.
    value(f, "ws_conn", "/last_bytes", QString{"ping from client"});
    REQUIRE(f.spin([&] { return !text.empty(); }));
    REQUIRE(text == QStringList{"echo:ping from client"});

    const QByteArray payload{"client\0binary", 13};
    peer.sendBinaryMessage(payload);
    value(f, "ws_conn", "/last_binary", QString::fromUtf8(payload));
    REQUIRE(f.spin([&] { return !binary.empty(); }));
    REQUIRE(binary.front() == payload);

    // Closing one connection from the script, not the whole server.
    f.push("ws_conn", "/disconnect", 1);
    REQUIRE(f.spin([&] { return disconnected; }));
    value(f, "ws_conn", "/status", QString{"client_closed"});
    f.removeMapper("ws_conn");
  });
}

TEST_CASE("Mapper WebSocket client runs peer messages on the Mapper thread", "[mapper][protocols][ws]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QWebSocketServer server{"Mapper peer", QWebSocketServer::NonSecureMode};
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    std::unique_ptr<QWebSocket> peer;
    QObject::connect(&server, &QWebSocketServer::newConnection, &server,
                     [&] { peer.reset(server.nextPendingConnection()); });
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("ws_affinity", wsClientScript(server.serverPort(), 3000));
    value(f, "ws_affinity", "/status", QString{"connected"});
    REQUIRE(f.spin([&] { return peer != nullptr; }));

    // Occupy the Mapper thread inside a script handler, then hand a message to
    // the websocket thread while it is still in there. The message callback runs
    // on the socket's own thread, so it cannot be interleaved: it is reported
    // after the busy handler returned, never inside it.
    f.push("ws_affinity", "/busy", 1);
    value(f, "ws_affinity", "/events", QString{"busy_start"});
    peer->sendTextMessage(QStringLiteral("peer_message"));
    value(f, "ws_affinity", "/events", QString{"busy_start,busy_end,peer_message"});
    f.removeMapper("ws_affinity");
  });
}

TEST_CASE("Mapper WebSocket client reports a close exactly once", "[mapper][protocols][ws]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QWebSocketServer server{"Mapper peer", QWebSocketServer::NonSecureMode};
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    std::unique_ptr<QWebSocket> peer;
    QObject::connect(&server, &QWebSocketServer::newConnection, &server,
                     [&] { peer.reset(server.nextPendingConnection()); });
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("ws_once", wsClientScript(server.serverPort(), 0));
    value(f, "ws_once", "/status", QString{"connected"});
    REQUIRE(f.spin([&] { return peer != nullptr; }));

    peer->close();
    value(f, "ws_once", "/status", QString{"disconnected"});
    value(f, "ws_once", "/close_count", 1);

    // Closing again, from the script this time: the connection is already gone,
    // so this reports nothing new.
    f.push("ws_once", "/disconnect", 1);
    REQUIRE_FALSE(f.spin(
        [&] { return f.contents("ws_once").value("ws_once:/close_count") != 1; }, 300));
    value(f, "ws_once", "/close_count", 1);
    f.removeMapper("ws_once");
  });
}

TEST_CASE("Mapper WebSocket server survives being closed from a connection callback", "[mapper][protocols][ws]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QTcpServer reservation;
    REQUIRE(reservation.listen(QHostAddress::LocalHost, 0));
    const auto target = reservation.serverPort();
    reservation.close();
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("ws_reentrant", wsReentrantCloseScript(target));
    ready(f, "ws_reentrant", "/status");

    QWebSocket peer;
    peer.open(QUrl{QStringLiteral("ws://127.0.0.1:%1").arg(target)});
    REQUIRE(f.spin([&] { return peer.state() == QAbstractSocket::ConnectedState; }));
    value(f, "ws_reentrant", "/status", QString{"client_connected"});

    // A message that changes nothing: the raw-frame callback is live.
    peer.sendTextMessage(QStringLiteral("warm up"));
    value(f, "ws_reentrant", "/last_bytes", QString{"warm up"});

    // The handler that receives this destroys the server, and with it the
    // connection object whose callback is running: on return, the connection is
    // still asked for its raw-frame callback, so it has to still be there.
    peer.sendTextMessage(QStringLiteral("close yourself"));
    value(f, "ws_reentrant", "/last_text", QString{"close yourself"});
    value(f, "ws_reentrant", "/status", QString{"closed"});
    REQUIRE(f.spin([&] { return peer.state() == QAbstractSocket::UnconnectedState; }));
    // The listener is released, so the port is rebindable again.
    REQUIRE(f.spin([&] { return reservation.listen(QHostAddress::LocalHost, target); }));
    f.removeMapper("ws_reentrant");
  });
}

TEST_CASE("Mapper Unix datagrams exchange bytes on temporary endpoints", "[mapper][protocols][unix]")
{
#if defined(Q_OS_UNIX)
  score::test::run_in_app([&](const auto& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto mapperPath = dir.filePath("mapper");
    const auto peerPath = dir.filePath("peer");
    unix_datagram peer{peerPath};
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_unix_datagram.qml");
    const QString original{"/tmp/ossia_test_dgram"};
    REQUIRE(script.count(original) == 2);
    substituteFirst(script, original, peerPath);
    substituteFirst(script, original, mapperPath);
    f.createMapper("unix_dgram", script);
    ready(f, "unix_dgram", "/send");
    REQUIRE(f.spin([&] { return QFileInfo::exists(mapperPath); }));
    const std::string bytes{"unix\0out", 8};
    f.push("unix_dgram", "/send", bytes);
    REQUIRE(peer.receive(f) == QByteArray(bytes.data(), bytes.size()));
    const QByteArray inbound{"unix\0in", 7};
    peer.send(mapperPath, inbound);
    value(f, "unix_dgram", "/last_message", QString::fromUtf8(inbound));
    peer.send(mapperPath, "next");
    value(f, "unix_dgram", "/last_message", QString{"next"});
    f.removeMapper("unix_dgram");
  });
#else
  SKIP("Unix-domain datagrams require a Unix platform with local socket support");
#endif
}

TEST_CASE("Mapper Unix stream connects, sends and counts independent clients", "[mapper][protocols][unix]")
{
#if defined(Q_OS_UNIX)
  score::test::run_in_app([&](const auto& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto mapperPath = dir.filePath("mapper");
    const auto peerPath = dir.filePath("peer");
    QLocalServer server;
    REQUIRE(server.listen(peerPath));
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_unix_stream.qml");
    const QString original{"/tmp/ossia_test_stream"};
    REQUIRE(script.count(original) == 2);
    substituteFirst(script, original, mapperPath);
    substituteFirst(script, original, peerPath);
    f.createMapper("unix_stream", script);
    // Both sockets report themselves from onOpen, which the factories run
    // after createTree() so that the writes reach the tree.
    value(f, "unix_stream", "/server_status", QString{"listening"});
    REQUIRE(f.spin([&] { return server.hasPendingConnections(); }));
    std::unique_ptr<QLocalSocket> peer{server.nextPendingConnection()};
    QByteArray received;
    REQUIRE(f.spin([&] { received += peer->readAll(); return received.size() >= 22; }));
    REQUIRE(received == "hello from unix stream");
    ready(f, "unix_stream", "/send");
    f.push("unix_stream", "/send", std::string{"second stream message"});
    received.clear();
    REQUIRE(f.spin([&] { received += peer->readAll(); return received.size() >= 21; }));
    REQUIRE(received == "second stream message");
    QLocalSocket first, second;
    first.connectToServer(mapperPath);
    REQUIRE(f.spin([&] { return first.state() == QLocalSocket::ConnectedState; }));
    value(f, "unix_stream", "/client_count", 1);
    second.connectToServer(mapperPath);
    REQUIRE(f.spin([&] { return second.state() == QLocalSocket::ConnectedState; }));
    value(f, "unix_stream", "/client_count", 2);
    // Peer EOF detection is covered by the next case.
    f.removeMapper("unix_stream");
    REQUIRE(f.spin([&] { return first.state() == QLocalSocket::UnconnectedState
                             && second.state() == QLocalSocket::UnconnectedState; }));
  });
#else
  SKIP("Unix-domain streams require a Unix platform with local socket support");
#endif
}

TEST_CASE("Mapper Unix stream client observes peer disconnection", "[mapper][protocols][unix]")
{
#if defined(Q_OS_UNIX)
  score::test::run_in_app([&](const auto& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto mapperPath = dir.filePath("mapper");
    const auto peerPath = dir.filePath("peer");
    QLocalServer server;
    REQUIRE(server.listen(peerPath));
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_unix_stream.qml");
    const QString original{"/tmp/ossia_test_stream"};
    REQUIRE(script.count(original) == 2);
    substituteFirst(script, original, mapperPath);
    substituteFirst(script, original, peerPath);
    f.createMapper("unix_stream_eof", script);
    REQUIRE(f.spin([&] { return server.hasPendingConnections(); }));
    std::unique_ptr<QLocalSocket> peer{server.nextPendingConnection()};
    QByteArray received;
    REQUIRE(f.spin([&] { received += peer->readAll(); return received.size() >= 22; }));
    REQUIRE(received == "hello from unix stream");
    value(f, "unix_stream_eof", "/client_status", QString{"connected"});
    peer->disconnectFromServer();
    REQUIRE(f.spin([&] { return peer->state() == QLocalSocket::UnconnectedState; }));
    value(f, "unix_stream_eof", "/client_status", QString{"disconnected"});
    f.removeMapper("unix_stream_eof");
  });
#else
  SKIP("Unix-domain streams require a Unix platform with local socket support");
#endif
}

TEST_CASE("Mapper Unix stream client reports a refused endpoint", "[mapper][protocols][unix]")
{
#if defined(Q_OS_UNIX)
  score::test::run_in_app([&](const auto& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto mapperPath = dir.filePath("mapper");
    // No server ever listens on the client's endpoint: connecting must fail.
    const auto peerPath = dir.filePath("absent");
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto script = f.script("test_unix_stream.qml");
    const QString original{"/tmp/ossia_test_stream"};
    REQUIRE(script.count(original) == 2);
    substituteFirst(script, original, mapperPath);
    substituteFirst(script, original, peerPath);
    f.createMapper("unix_stream_fail", script);
    // The script spells its error handler onFail, the documented alias.
    value(f, "unix_stream_fail", "/client_status", QString{"failed"});
    f.removeMapper("unix_stream_fail");
  });
#else
  SKIP("Unix-domain streams require a Unix platform with local socket support");
#endif
}

#if defined(Q_OS_UNIX)
namespace
{
//! A pty pair: the script opens the slave, the test drives the master.
struct pty_pair
{
  int master{-1};
  std::string slave;

  pty_pair()
  {
    master = ::posix_openpt(O_RDWR | O_NOCTTY);
    if(master < 0)
      return;
    if(::grantpt(master) != 0 || ::unlockpt(master) != 0)
      return;
    if(const char* name = ::ptsname(master))
      slave = name;
  }
  ~pty_pair()
  {
    if(master >= 0)
      ::close(master);
  }

  bool valid() const { return master >= 0 && !slave.empty(); }
};

// No onMessage and no onBytes: nothing will ever be dispatched, so the read
// loop exists purely to keep the port's own notifications flowing.
QString serialNotificationScript(const std::string& port)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var sock: null
  property string events: ""

  function note(tag) {
    events = (events.length === 0) ? tag : (events + "," + tag);
    Device.write("/events", events);
  }

  function createTree() {
    sock = Protocols.serial({
      Transport: { Port: "%1", Baud: 115200 },
      onOpen: function(s) { note("open"); },
      onError: function() { note("error"); },
      onClose: function() { note("close"); }
    });

    return [
      { name: "events", type: Ossia.Type.String, value: "" },
      { name: "shutdown", type: Ossia.Type.Int,
        write: function(v) { sock.close(); } }
    ];
  }
}
)qml")
      .arg(QString::fromStdString(port));
}
}
#endif

TEST_CASE("Mapper serial port reports its stream notifications without a message callback", "[mapper][protocols][serial]")
{
#if defined(Q_OS_UNIX)
  pty_pair pty;
  if(!pty.valid())
    SKIP("could not open a pty pair");

  score::test::run_in_app([&](const auto& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("serial_events", serialNotificationScript(pty.slave));
    value(f, "serial_events", "/events", QString{"open"});

    // Cancelling the port completes the pending read with operation_aborted,
    // which is the script's onError. It can only be reported if a read was
    // armed, and nothing here ever asked for a message.
    f.push("serial_events", "/shutdown", 1);
    const bool reported = f.spin([&] {
      const auto events
          = f.contents("serial_events").value("serial_events:/events").toString();
      return events.contains("error") && events.contains("close");
    });
    INFO("events: " << f.contents("serial_events")
                           .value("serial_events:/events")
                           .toString()
                           .toStdString());
    REQUIRE(reported);
    f.removeMapper("serial_events");
  });
#else
  SKIP("Serial ports need a pty pair, which requires a Unix platform");
#endif
}

namespace
{
// Only the endpoint is replaced: default QML enumeration excludes virtual
// ports, and selecting devices[0] could send test traffic to user hardware.
// Every field below mirrors midi_port_information(), so the substituted object
// is what Protocols.*Devices() would have produced for this port, and
// qjs_to_midi_port_information() reads it back as such.
QString midiEndpointScript(const QString& filename, const libremidi::port_information& p,
                           const QString& enumeration)
{
  static constexpr std::pair<libremidi::transport_type, const char*> transports[]{
      {libremidi::transport_type::software, "Software"},
      {libremidi::transport_type::loopback, "Loopback"},
      {libremidi::transport_type::hardware, "Hardware"},
      {libremidi::transport_type::usb, "USB"},
      {libremidi::transport_type::bluetooth, "Bluetooth"},
      {libremidi::transport_type::pci, "PCI"},
      {libremidi::transport_type::network, "Network"}};
  QJsonArray type;
  for(auto [flag, name] : transports)
    if(static_cast<bool>(p.type & flag))
      type.append(QString::fromUtf8(name));

  const auto api = libremidi::get_api_name(p.api);
  QJsonObject endpoint{{"Name", QString::fromStdString(p.port_name)},
                       {"DisplayName", QString::fromStdString(p.display_name)},
                       {"Manufacturer", QString::fromStdString(p.manufacturer)},
                       {"DeviceName", QString::fromStdString(p.device_name)},
                       {"API", QString::fromUtf8(api.data(), api.size())},
                       {"Type", type},
                       {"PortHandle", QString::number(p.port)},
                       {"ClientHandle", QString::number(p.client)}};
  if(auto s = get_if<std::string>(&p.device))
    endpoint["DeviceID"] = QString::fromStdString(*s);
  if(auto n = get_if<std::uint64_t>(&p.device))
    endpoint["DeviceID"] = QString::number(*n);
  if(auto s = get_if<std::string>(&p.container))
    endpoint["ContainerID"] = QString::fromStdString(*s);
  if(auto n = get_if<std::uint64_t>(&p.container))
    endpoint["ContainerID"] = QString::number(*n);
  if(auto id = get_if<libremidi::uuid>(&p.container))
  {
    QJsonArray bytes;
    for(auto b : id->bytes)
      bytes.append(b);
    endpoint["ContainerID"] = bytes;
  }
  auto script = fixture::script(filename);
  substitute(script, enumeration,
             QString::fromUtf8(QJsonDocument{QJsonArray{endpoint}}.toJson(QJsonDocument::Compact)));
  return script;
}

struct virtual_midi
{
  const libremidi::API api;
  const std::string name = "score_mapper_" + QUuid::createUuid().toString(QUuid::Id128).toStdString();
  std::mutex mutex;
  std::vector<std::vector<unsigned char>> messages;
  std::vector<std::array<uint32_t, 2>> packets;
  std::unique_ptr<libremidi::midi_in> input;
  std::unique_ptr<libremidi::midi_out> output;
  std::unique_ptr<libremidi::observer> observer;
  // No native backend at all: an environment fact, and the only honest skip.
  std::string unavailable;
  // A backend is there but did not do its job: a failure. Skipping here would
  // let the whole MIDI half of the suite go green on a box where virtual MIDI
  // is broken, or after a libremidi regression.
  std::string broken;

  explicit virtual_midi(bool ump, bool mapperSends)
      : api{ump ? libremidi::midi2::default_api() : libremidi::midi1::default_api()}
  {
    if(api == libremidi::API::DUMMY)
    {
      unavailable = "libremidi default API is DUMMY: no native MIDI backend compiled";
      return;
    }
    try
    {
      libremidi::observer_configuration observation;
      observation.track_virtual = true;
      observation.track_any = true;
      observer = std::make_unique<libremidi::observer>(observation, api);
      stdx::error error;
      if(mapperSends)
      {
        if(ump)
        {
          libremidi::ump_input_configuration config;
          config.on_message = [this](const libremidi::ump& msg) {
            std::lock_guard lock{mutex};
            packets.push_back({msg.data[0], msg.data[1]});
          };
          input = std::make_unique<libremidi::midi_in>(config, api);
        }
        else
        {
          libremidi::input_configuration config;
          config.ignore_sysex = false;
          config.on_message = [this](const libremidi::message& msg) {
            std::lock_guard lock{mutex};
            messages.emplace_back(msg.bytes.begin(), msg.bytes.end());
          };
          input = std::make_unique<libremidi::midi_in>(config, api);
        }
        error = input->open_virtual_port(name);
      }
      else
      {
        output = std::make_unique<libremidi::midi_out>(libremidi::output_configuration{}, api);
        error = output->open_virtual_port(name);
      }
      if(error != stdx::error{})
      {
        const auto message = error.message();
        broken.assign(message.data(), message.size());
      }
    }
    catch(const std::exception& e)
    {
      broken = e.what();
    }
  }

  std::optional<libremidi::port_information> endpoint(bool mapperSends)
  {
    if(mapperSends)
    {
      for(const auto& p : observer->get_output_ports())
        if(p.port_name.find(name) != std::string::npos)
          return p;
    }
    else
    {
      for(const auto& p : observer->get_input_ports())
        if(p.port_name.find(name) != std::string::npos)
          return p;
    }
    return {};
  }
};

//! Skips only when there is no native MIDI backend to test against.
void require_midi(const virtual_midi& peer)
{
  if(!peer.broken.empty())
    FAIL("the native MIDI backend refused to open a virtual port: " << peer.broken);
  if(!peer.unavailable.empty())
    SKIP("No native MIDI backend: " << peer.unavailable);
}

ossia::value list(std::initializer_list<int> values)
{
  std::vector<ossia::value> result;
  for(auto v : values)
    result.emplace_back(v);
  return result;
}
}

TEST_CASE("Mapper MIDI input reads real virtual MIDI messages", "[mapper][protocols][midi]")
{
  score::test::run_in_app([&](const auto& ctx) {
    virtual_midi peer{false, false};
    require_midi(peer);
    std::optional<libremidi::port_information> endpoint;
    REQUIRE(fixture::spin([&] { endpoint = peer.endpoint(false); return endpoint.has_value(); }));
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("midi_in", midiEndpointScript("test_midi_inbound.qml", *endpoint,
                                               "Protocols.inboundMIDIDevices()"));
    // onOpen runs before createTree(): the write only lands because the
    // factory defers the open. It is also what says the port is ready.
    value(f, "midi_in", "/status", QString{"open"});
    ready(f, "midi_in", "/last_status");
    REQUIRE(peer.output->send_message(0x92, 64, 101) == stdx::error{});
    value(f, "midi_in", "/last_status", 0x92);
    value(f, "midi_in", "/last_data1", 64);
    value(f, "midi_in", "/last_data2", 101);
    REQUIRE(peer.output->send_message(0xc2, 17) == stdx::error{});
    value(f, "midi_in", "/last_status", 0xc2);
    value(f, "midi_in", "/last_data1", 17);
    value(f, "midi_in", "/last_data2", 101);
    REQUIRE(peer.output->send_message(0xf8) == stdx::error{});
    value(f, "midi_in", "/last_status", 0xf8);
    value(f, "midi_in", "/last_data1", 17);
    f.removeMapper("midi_in");
  });
}

TEST_CASE("Mapper MIDI output emits note control program and raw messages", "[mapper][protocols][midi]")
{
  score::test::run_in_app([&](const auto& ctx) {
    virtual_midi peer{false, true};
    require_midi(peer);
    std::optional<libremidi::port_information> endpoint;
    REQUIRE(fixture::spin([&] { endpoint = peer.endpoint(true); return endpoint.has_value(); }));
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("midi_out", midiEndpointScript("test_midi_outbound.qml", *endpoint,
                                                "Protocols.outboundMIDIDevices()"));
    value(f, "midi_out", "/status", QString{"open"});
    ready(f, "midi_out", "/note_on");
    f.push("midi_out", "/note_on", list({3, 64, 101}));
    f.push("midi_out", "/note_off", list({3, 64, 45}));
    f.push("midi_out", "/cc", list({16, 7, 99}));
    f.push("midi_out", "/program", list({2, 17}));
    f.push("midi_out", "/raw", list({0xf0, 0x7d, 1, 2, 0xf7}));
    INFO("tree: " << QJsonDocument::fromVariant(f.contents("midi_out")).toJson().toStdString());
    REQUIRE(f.spin([&] { std::lock_guard lock{peer.mutex}; return peer.messages.size() >= 5; }));
    const std::vector<std::vector<unsigned char>> expected{
        {0x92, 64, 101}, {0x82, 64, 45}, {0xbf, 7, 99}, {0xc1, 17}, {0xf0, 0x7d, 1, 2, 0xf7}};
    {
      std::lock_guard lock{peer.mutex};
      REQUIRE(peer.messages == expected);
    }
    f.removeMapper("midi_out");
  });
}

TEST_CASE("Mapper UMP input reads native virtual MIDI two word messages", "[mapper][protocols][ump]")
{
  score::test::run_in_app([&](const auto& ctx) {
    virtual_midi peer{true, false};
    require_midi(peer);
    std::optional<libremidi::port_information> endpoint;
    REQUIRE(fixture::spin([&] { endpoint = peer.endpoint(false); return endpoint.has_value(); }));
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("ump_in", midiEndpointScript("test_ump_inbound.qml", *endpoint,
                                              "Protocols.inboundUMPDevices()"));
    value(f, "ump_in", "/status", QString{"open"});
    ready(f, "ump_in", "/last_word0");
    REQUIRE(peer.output->send_ump(uint32_t{0x42934001}, uint32_t{0x12345678}) == stdx::error{});
    value(f, "ump_in", "/last_word0", int{0x42934001});
    value(f, "ump_in", "/last_word1", int{0x12345678});
    REQUIRE(peer.output->send_ump(uint32_t{0x42834000}, uint32_t{0x23450000}) == stdx::error{});
    value(f, "ump_in", "/last_word0", int{0x42834000});
    value(f, "ump_in", "/last_word1", int{0x23450000});
    f.removeMapper("ump_in");
  });
}

TEST_CASE("Mapper UMP output emits exact MIDI two note control and raw words", "[mapper][protocols][ump]")
{
  score::test::run_in_app([&](const auto& ctx) {
    virtual_midi peer{true, true};
    require_midi(peer);
    std::optional<libremidi::port_information> endpoint;
    REQUIRE(fixture::spin([&] { endpoint = peer.endpoint(true); return endpoint.has_value(); }));
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("ump_out", midiEndpointScript("test_ump_outbound.qml", *endpoint,
                                               "Protocols.outboundUMPDevices()"));
    value(f, "ump_out", "/status", QString{"open"});
    ready(f, "ump_out", "/note_on");
    f.push("ump_out", "/note_on", list({3, 4, 64, 0x1234, 1, 0x5678}));
    f.push("ump_out", "/note_off", list({3, 4, 64, 0x2345, 0, 0}));
    f.push("ump_out", "/cc", list({2, 16, 7, 0x12345678}));
    f.push("ump_out", "/raw", list({0x40913c00, 0x45670000, 0, 0}));
    INFO("tree: " << QJsonDocument::fromVariant(f.contents("ump_out")).toJson().toStdString());
    REQUIRE(f.spin([&] { std::lock_guard lock{peer.mutex}; return peer.packets.size() >= 4; }));
    const std::vector<std::array<uint32_t, 2>> expected{
        {0x42934001, 0x12345678}, {0x42834000, 0x23450000},
        {0x41bf0700, 0x12345678}, {0x40913c00, 0x45670000}};
    {
      std::lock_guard lock{peer.mutex};
      REQUIRE(peer.packets == expected);
    }
    f.removeMapper("ump_out");
  });
}

namespace
{
// Nothing is substituted in these two scripts: they enumerate through the
// production Protocols.*MIDIDevices() and hand the object they get straight
// back to the production factory, so the round trip - midi_port_information()
// writing the endpoint out, qjs_to_midi_port_information() reading it back - is
// what is under test.
QString enumeratedInboundMidiScript(const QString& name)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var endpoints: Protocols.inboundMIDIDevices().filter(
    function(p) { return p.Name.indexOf("%1") >= 0; })

  property var port: endpoints.length === 1
    ? Protocols.inboundMIDI({
        Transport: endpoints[0],
        onOpen: function(socket) { Device.write("/status", "open"); },
        onError: function(err) { Device.write("/status", "error:" + err); },
        onMessage: function(msg) { Device.write("/last_status", msg.bytes[0]); }
      })
    : null

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String,
        value: "endpoints:" + endpoints.length },
      { name: "last_status", type: Ossia.Type.Int, value: 0 }
    ];
  }
}
)qml").arg(name);
}

QString enumeratedOutboundMidiScript(const QString& name)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var endpoints: Protocols.outboundMIDIDevices().filter(
    function(p) { return p.Name.indexOf("%1") >= 0; })

  property var port: endpoints.length === 1
    ? Protocols.outboundMIDI({
        Transport: endpoints[0],
        onOpen: function(socket) { Device.write("/status", "open"); },
        onError: function(err) { Device.write("/status", "error:" + err); }
      })
    : null

  function createTree() {
    return [
      { name: "status", type: Ossia.Type.String,
        value: "endpoints:" + endpoints.length },
      { name: "raw", type: Ossia.Type.List,
        write: function(v) {
          var bytes = [];
          for(var i = 0; i < v.value.length; i++)
            bytes.push(v.value[i].value);
          port.sendMessage(bytes);
        } }
    ];
  }
}
)qml").arg(name);
}
}

TEST_CASE("Mapper reopens a MIDI input it enumerated itself", "[mapper][protocols][midi]")
{
  score::test::run_in_app([&](const auto& ctx) {
    virtual_midi peer{false, false};
    require_midi(peer);
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("midi_roundtrip_in",
                   enumeratedInboundMidiScript(QString::fromStdString(peer.name)));
    // "open" means the API string the enumeration produced was understood by
    // the parser: any other spelling leaves the endpoint unopenable.
    value(f, "midi_roundtrip_in", "/status", QString{"open"});
    REQUIRE(peer.output->send_message(0x92, 64, 101) == stdx::error{});
    value(f, "midi_roundtrip_in", "/last_status", 0x92);
    f.removeMapper("midi_roundtrip_in");
  });
}

TEST_CASE("Mapper reopens a MIDI output it enumerated itself", "[mapper][protocols][midi]")
{
  score::test::run_in_app([&](const auto& ctx) {
    virtual_midi peer{false, true};
    require_midi(peer);
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("midi_roundtrip_out",
                   enumeratedOutboundMidiScript(QString::fromStdString(peer.name)));
    value(f, "midi_roundtrip_out", "/status", QString{"open"});
    ready(f, "midi_roundtrip_out", "/raw");
    f.push("midi_roundtrip_out", "/raw", list({0x92, 64, 101}));
    REQUIRE(f.spin([&] { std::lock_guard lock{peer.mutex}; return !peer.messages.empty(); }));
    {
      std::lock_guard lock{peer.mutex};
      REQUIRE(peer.messages
              == std::vector<std::vector<unsigned char>>{{0x92, 64, 101}});
    }
    f.removeMapper("midi_roundtrip_out");
  });
}

namespace
{
QString midiTransportScript(const QString& transport)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var port: Protocols.inboundMIDI({
    Transport: %1,
    onOpen: function(socket) { Device.write("/error", "opened"); },
    onError: function(err) { Device.write("/error", err); }
  })

  function createTree() {
    return [ { name: "error", type: Ossia.Type.String, value: "" } ];
  }
}
)qml").arg(transport);
}
}

TEST_CASE("Mapper MIDI endpoint errors name the omission or the wrong value",
          "[mapper][protocols][midi]")
{
  score::test::run_in_app([&](const auto& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    // A Transport without an API is the mistake of hand-writing one instead of
    // enumerating, and the message has to say which functions to call.
    f.createMapper("midi_no_api", midiTransportScript("{ }"));
    value(f, "midi_no_api", "/error",
          QString{"Transport has no API: use one of the endpoints returned by "
                  "Protocols.inbound/outboundMIDIDevices() or "
                  "Protocols.inbound/outboundUMPDevices()"});
    f.removeMapper("midi_no_api");

    // A Transport with an API that does not exist is a different mistake and
    // gets the value echoed back.
    f.createMapper("midi_bad_api", midiTransportScript(R"({ API: "not-an-api" })"));
    value(f, "midi_bad_api", "/error", QString{"Unknown MIDI API: not-an-api"});
    f.removeMapper("midi_bad_api");
  });
}

namespace
{
QString bothErrorSpellingsScript(const QString& path)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var client: Protocols.outboundUnixStream({
    Transport: { Path: "%1" },
    onError: function() { Device.write("/status", "onError"); },
    onFail: function() { Device.write("/status", "onFail"); }
  })

  function createTree() {
    return [ { name: "status", type: Ossia.Type.String, value: "idle" } ];
  }
}
)qml").arg(path);
}
}

TEST_CASE("Mapper socket error callbacks honour the canonical spelling",
          "[mapper][protocols][unix]")
{
#if defined(Q_OS_UNIX)
  score::test::run_in_app([&](const auto& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    // onError and onFail are two names for one callback. A configuration that
    // carries both is a mistake, and the failure must still be reported
    // exactly once, through the canonical spelling.
    f.createMapper("both_spellings", bothErrorSpellingsScript(dir.filePath("absent")));
    value(f, "both_spellings", "/status", QString{"onError"});
    f.removeMapper("both_spellings");
  });
#else
  SKIP("Unix-domain streams require a Unix platform with local socket support");
#endif
}

namespace
{
// Two mappers: the writer names an address in the sink's tree, which the engine
// resolves once and then caches as a raw parameter pointer.
QString cacheSinkScript()
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  function createTree() {
    return [ { name: "target", type: Ossia.Type.String, value: "" } ];
  }
}
)qml");
}

QString cacheWriterScript()
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  function createTree() {
    return [
      { name: "go", type: Ossia.Type.String,
        write: function(v) { Device.write("cache_sink:/target", v.value); } }
    ];
  }
}
)qml");
}
}

TEST_CASE("Mapper resolved addresses do not outlive the device they named", "[mapper][protocols]")
{
  score::test::run_in_app([&](const auto& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("cache_sink", cacheSinkScript());
    f.createMapper("cache_writer", cacheWriterScript());
    ready(f, "cache_sink", "/target");
    ready(f, "cache_writer", "/go");

    // Writing once is what puts the sink's parameter into the writer's cache.
    // The push is retried because the writer only learns about the sink when
    // the device list reaches its thread.
    REQUIRE(f.spin([&] {
      f.push("cache_writer", "/go", std::string{"first"});
      return f.contents("cache_sink").value("cache_sink:/target")
             == QVariant{QString{"first"}};
    }));

    // Same device name, new device, new parameter: everything the cache holds
    // for that address is freed here.
    f.removeMapper("cache_sink");
    f.createMapper("cache_sink", cacheSinkScript());
    ready(f, "cache_sink", "/target");

    REQUIRE(f.spin([&] {
      f.push("cache_writer", "/go", std::string{"second"});
      return f.contents("cache_sink").value("cache_sink:/target")
             == QVariant{QString{"second"}};
    }));
    f.removeMapper("cache_writer");
    f.removeMapper("cache_sink");
  });
}
