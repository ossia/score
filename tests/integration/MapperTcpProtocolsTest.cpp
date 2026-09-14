#include <core/document/Document.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Mapper.hpp>

#include <ossia/network/value/value.hpp>
#include <catch2/catch_all.hpp>

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(Q_OS_UNIX)
#include <sys/socket.h>
#endif

namespace
{
using score::test::mapper::fixture;
constexpr auto device = "tcp_scenario";

// The observable effect a fed line must have: a device value it changes.
using effect = std::pair<QString, QVariant>;

// A denied dispatch is only ever proven by an absence, so the window has to be
// wide enough that a real dispatch would have been seen inside it: every script
// here reacts to a complete message within one fixture::spin poll.
constexpr int denied_ms = 200;

// A reply is written by the handler that ran, within one event-loop turn, so
// trailing bytes are already queued by the time the expected ones arrive.
constexpr int drain_ms = 40;

void replaceOnce(QString& qml, const QString& from, const QString& to)
{
  REQUIRE(qml.count(from) == 1);
  qml.replace(from, to);
}

QVariant value(fixture& f, const QString& path)
{
  return f.contents(device).value(QString{device} + ":" + path);
}

void expectValue(fixture& f, const QString& path, const QVariant& expected)
{
  const bool reached = f.spin([&] { return value(f, path) == expected; });
  INFO("Mapper path: " << path.toStdString()
                       << " expected: " << expected.toString().toStdString()
                       << " actual: " << value(f, path).toString().toStdString());
  REQUIRE(reached);
}

void listen(QTcpServer& server)
{
  REQUIRE(server.listen(QHostAddress::LocalHost, 0));
}

std::unique_ptr<QTcpSocket> accept(fixture& f, QTcpServer& server)
{
  REQUIRE(f.spin([&] { return server.hasPendingConnections(); }));
  return std::unique_ptr<QTcpSocket>{server.nextPendingConnection()};
}

void connect(fixture& f, QTcpSocket& socket, quint16 port)
{
  socket.connectToHost(QHostAddress::LocalHost, port);
  REQUIRE(f.spin([&] { return socket.state() == QAbstractSocket::ConnectedState; }));
}

void send(fixture& f, QTcpSocket& socket, const QByteArray& bytes)
{
  REQUIRE(socket.write(bytes) == bytes.size());
  REQUIRE(f.spin([&] { return socket.bytesToWrite() == 0; }));
}

QByteArray take(fixture& f, QTcpSocket& socket, const QByteArray& expected)
{
  const bool arrived = f.spin([&] { return socket.bytesAvailable() >= expected.size(); });
  auto actual = socket.read(arrived ? expected.size() : socket.bytesAvailable());
  INFO("expected wire: " << expected.toHex().toStdString());
  INFO("actual wire: " << actual.toHex().toStdString());
  INFO("actual text: " << actual.toStdString());
  REQUIRE(arrived);
  REQUIRE(actual == expected);
  // The expected bytes are the whole write: `expected` followed by anything
  // else is a different wire, which a prefix comparison would accept.
  const bool drained = !f.spin([&] { return socket.bytesAvailable() > 0; }, drain_ms);
  INFO("stray wire: " << socket.peek(socket.bytesAvailable()).toHex().toStdString());
  REQUIRE(drained);
  return actual;
}

// Independent encoders: do not let the production decoder and encoder agree
// on the same wrong representation. TCP reads are never treated as messages.
QByteArray be32(std::uint32_t n)
{
  QByteArray out;
  for(int shift : {24, 16, 8, 0})
    out += char(n >> shift);
  return out;
}

QByteArray oscString(QByteArray text)
{
  text += '\0';
  while(text.size() % 4)
    text += '\0';
  return text;
}

QByteArray osc(const QByteArray& address, const QByteArray& tags = ",",
               const QByteArray& arguments = {})
{
  return oscString(address) + oscString(tags) + arguments;
}

QByteArray slip(const QByteArray& data)
{
  QByteArray out(1, char(0xc0));
  for(unsigned char c : data)
  {
    if(c == 0xc0)
      out += QByteArray::fromHex("dbdc");
    else if(c == 0xdb)
      out += QByteArray::fromHex("dbdd");
    else
      out += char(c);
  }
  return out + char(0xc0);
}

QByteArray ascii85(const QByteArray& data)
{
  QByteArray out;
  for(qsizetype pos = 0; pos < data.size(); pos += 4)
  {
    const auto count = std::min<qsizetype>(4, data.size() - pos);
    std::uint32_t n{};
    for(int j = 0; j < 4; j++)
      n = (n << 8) | (j < count ? static_cast<unsigned char>(data[pos + j]) : 0);
    if(count == 4 && n == 0)
      out += 'z';
    else
    {
      char digits[5];
      for(int j = 4; j >= 0; j--)
      {
        digits[j] = char('!' + n % 85);
        n /= 85;
      }
      out.append(digits, count + 1);
    }
  }
  return out;
}

// Firmware records, chunked exactly like the production encoders: 16 payload
// bytes per record, 16-bit load address incremented by the chunk size, CRLF
// between records (ihex_encode / srec_encode).
// EOF is deliberately separate: it is a record of its own, and it decodes to
// no payload at all, which is not a message.
QByteArray record(const QByteArray& data, bool motorola)
{
  QByteArray out;
  for(qsizetype pos = 0; pos < data.size(); pos += 16)
  {
    const auto chunk = data.mid(pos, 16);
    QByteArray bytes;
    bytes += char(chunk.size() + (motorola ? 3 : 0));
    bytes += char(pos >> 8);
    bytes += char(pos);
    if(!motorola)
      bytes += '\0';
    bytes += chunk;
    unsigned int sum{};
    for(unsigned char c : bytes)
      sum += c;
    bytes += char(motorola ? ~sum : -sum);
    if(!out.isEmpty())
      out += "\r\n";
    out += (motorola ? QByteArray{"S1"} : QByteArray{":"}) + bytes.toHex().toUpper();
  }
  return out;
}

enum class framing { raw, lf, crlf, size, slip };
enum class encoding { raw, base64, hex, ascii85, ihex, srec };

QByteArray frame(QByteArray bytes, framing type, encoding enc = encoding::raw)
{
  switch(enc)
  {
    case encoding::raw: break;
    case encoding::base64: bytes = bytes.toBase64(); break;
    case encoding::hex: bytes = bytes.toHex().toUpper(); break;
    case encoding::ascii85: bytes = ascii85(bytes); break;
    case encoding::ihex: bytes = record(bytes, false) + "\r\n:00000001FF\r\n"; break;
    case encoding::srec: bytes = record(bytes, true) + "\r\nS9030000FC\r\n"; break;
  }
  switch(type)
  {
    case framing::raw: return bytes;
    case framing::lf: return bytes + '\n';
    case framing::crlf: return bytes + "\r\n";
    case framing::size: return be32(bytes.size()) + bytes;
    case framing::slip: return slip(bytes);
  }
  return {};
}

// The two loopback endpoints are interposed by independent Qt TCP peers. The
// test forwards captured client frames to the original server, then forwards
// its replies back to the original client. Only endpoint literals change.
struct loopback
{
  QTcpServer clientEndpoint;
  QTcpSocket serverPeer;
  std::unique_ptr<QTcpSocket> clientPeer;

  loopback(fixture& f, QString qml, int originalPort)
  {
    listen(clientEndpoint);
    QTcpServer reservation;
    listen(reservation);
    const auto port = reservation.serverPort();
    replaceOnce(qml, QString{"Bind: \"127.0.0.1\", Port: %1"}.arg(originalPort),
                QString{"Bind: \"127.0.0.1\", Port: %1"}.arg(port));
    replaceOnce(qml, QString{"Host: \"127.0.0.1\", Port: %1"}.arg(originalPort),
                QString{"Host: \"127.0.0.1\", Port: %1"}.arg(clientEndpoint.serverPort()));
    reservation.close();
    f.createMapper(device, qml);
    clientPeer = accept(f, clientEndpoint);
    connect(f, serverPeer, port);
  }
};

struct hardware
{
  QTcpServer server;
  std::unique_ptr<QTcpSocket> peer;
  QByteArray delimiter;

  // The delimiter is the one the vendored script configures for its line
  // framing, which is not always the one the emulated device sends.
  hardware(fixture& f, const QString& filename, int originalPort,
           QByteArray scriptDelimiter = "\r\n")
      : delimiter{std::move(scriptDelimiter)}
  {
    listen(server);
    auto qml = f.script(filename);
    replaceOnce(qml, QString{"Host: host, Port: %1"}.arg(originalPort),
                QString{"Host: host, Port: %1"}.arg(server.serverPort()));
    f.createMapper(device, qml);
    REQUIRE(f.spin([&] { return f.contents(device).contains(QString{device} + ":/connect"); }));
    f.push(device, "/connect", std::string{"127.0.0.1"});
    peer = accept(f, server);
  }

  void command(fixture& f, const QString& path, const ossia::value& v,
               const QByteArray& expected)
  {
    f.push(device, path, v);
    take(f, *peer, expected);
  }

  //! Feed one message, split in two writes inside the first line.
  /** Whatever the framing, a fragment is not a message: nothing may be
   * dispatched before the rest arrives. A line-framed script gets exactly one
   * line, which is always the first line of its read and therefore safe.
   *
   * Almost none of these response handlers answer on the socket -- they only
   * write device values -- so a quiet socket proves nothing at all about them.
   * The effects the complete line is about to have are snapshotted instead:
   * the window asserts that not one of them has happened yet, and the line is
   * then required to produce all of them.
   */
  void reply(fixture& f, const QByteArray& bytes, std::initializer_list<effect> effects)
  {
    REQUIRE(effects.size() > 0);
    fragment(f, bytes, effects);
    for(const auto& [path, expected] : effects)
      expectValue(f, path, expected);
  }

  //! Feed one message whose only observable effect is the answer it provokes.
  void handshake(fixture& f, const QByteArray& bytes, const QByteArray& answer)
  {
    fragment(f, bytes, {});
    take(f, *peer, answer);
  }

  //! The incomplete first line: neither the wire nor any awaited value may move.
  void fragment(fixture& f, const QByteArray& bytes, std::initializer_list<effect> effects)
  {
    if(!delimiter.isEmpty())
      REQUIRE(bytes.indexOf(delimiter) + delimiter.size() == bytes.size());
    // A value that already holds what the line is about to write would make
    // the window below assert nothing.
    std::vector<QVariant> before;
    for(const auto& [path, expected] : effects)
    {
      INFO("Mapper path: " << path.toStdString());
      REQUIRE(value(f, path) != expected);
      before.push_back(value(f, path));
    }
    INFO("Denied fragment: " << bytes.toStdString());
    send(f, *peer, bytes.left(2));
    REQUIRE_FALSE(f.spin(
        [&] {
          if(peer->bytesAvailable() > 0)
            return true;
          const auto now = f.contents(device);
          for(std::size_t i = 0; i < before.size(); i++)
            if(now.value(QString{device} + ":" + (effects.begin() + i)->first) != before[i])
              return true;
          return false;
        },
        denied_ms));
    send(f, *peer, bytes.mid(2));
  }

  //! One line, paced by an observable effect of the previous one.
  /** Lines that coalesce into a single read are all dispatched, in order (its
   * own case below asserts exactly that); pacing here only keeps each
   * assertion attributable to the line that caused it.
   */
  void line(fixture& f, const QByteArray& bytes)
  {
    REQUIRE(bytes.indexOf(delimiter) + delimiter.size() == bytes.size());
    send(f, *peer, bytes);
  }

  //! Feed a whole block in a single write.
  /** Block headers and blank terminators run no observable script code, so
   * nothing acknowledges them one by one: the block is handed over as one
   * coalesced read, which the line decoder must split back into its lines.
   */
  void block(fixture& f, std::initializer_list<QByteArray> lines, const QString& path,
             const QVariant& expected)
  {
    INFO("Block awaited on: " << path.toStdString());
    QByteArray bytes;
    for(const auto& part : lines)
    {
      REQUIRE(part.indexOf(delimiter) + delimiter.size() == part.size());
      bytes += part;
    }
    send(f, *peer, bytes);
    expectValue(f, path, expected);
  }
};

#if defined(Q_OS_UNIX)
//! An abortive close: SO_LINGER with a zero timeout makes the kernel answer the
//! close with RST instead of FIN, which the reader sees as connection_reset.
void reset(fixture& f, QTcpSocket& socket)
{
  const ::linger option{1, 0};
  REQUIRE(
      ::setsockopt(
          socket.socketDescriptor(), SOL_SOCKET, SO_LINGER, &option, sizeof(option))
      == 0);
  socket.abort();
  REQUIRE(f.spin([&] { return socket.state() == QAbstractSocket::UnconnectedState; }));
}
#endif
}

TEST_CASE("Mapper TCP framing and text encodings cross independent peers", "[mapper][protocols][tcp]")
{
  struct scenario
  {
    const char* file;
    int port;
    framing framingType;
    encoding encodingType;
    std::vector<QByteArray> messages;
    QByteArray prefix;
    QByteArray constantReply;
    const char* pushPath;
  };
  const scenario scenarios[]{
      {"test_tcp_slip_loopback.qml", 5600, framing::slip, encoding::raw,
       {"hello-slip"}, "echo:", {}, "/send"},
      {"test_tcp_size_prefix_loopback.qml", 5601, framing::size, encoding::raw,
       {"msg-one", "msg-two"}, "ack:", {}, nullptr},
      {"test_tcp_line_loopback.qml", 5602, framing::crlf, encoding::raw,
       {"HELLO", "WORLD"}, "OK:", {}, nullptr},
      {"test_tcp_base64_loopback.qml", 5610, framing::lf, encoding::base64,
       {"hello base64", QByteArray{"binary-safe: "} + QByteArray::fromHex("000102c3bf")},
       "echo:", {}, "/send"},
      {"test_tcp_hex_loopback.qml", 5611, framing::crlf, encoding::hex,
       {"Hello", "World"}, {}, "OK", "/send"},
      {"test_tcp_ascii85_loopback.qml", 5612, framing::size, encoding::ascii85,
       {"compact encoding", QByteArray(4, '\0')}, "re:", {}, nullptr},
  };
  for(const auto& s : scenarios)
  {
    DYNAMIC_SECTION(s.file)
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto doc = score::test::new_document(ctx);
        REQUIRE(doc);
        fixture f{ctx, *doc};
        loopback p{f, f.script(s.file), s.port};
        QByteArray startup;
        QByteArray replies;
        for(const auto& message : s.messages)
        {
          startup += frame(message, s.framingType, s.encodingType);
          replies += frame(s.constantReply.isEmpty() ? s.prefix + message : s.constantReply,
                           s.framingType, s.encodingType);
        }
        take(f, *p.clientPeer, startup);
        // First frame is incomplete, so no callback or reply can run yet.
        send(f, p.serverPeer, startup.left(2));
        REQUIRE_FALSE(f.spin([&] { return p.serverPeer.bytesAvailable() > 0; }, denied_ms));
        // Every frame of the read at once: the decoders must dispatch all of
        // them, in order, and keep any trailing partial frame for later.
        send(f, p.serverPeer, startup.mid(2));
        take(f, p.serverPeer, replies);
        send(f, *p.clientPeer, replies);
        const auto last = s.messages.back();
        expectValue(f, "/server_received", QString::fromUtf8(last));
        expectValue(f, "/client_received",
                    QString::fromUtf8(s.constantReply.isEmpty() ? s.prefix + last : s.constantReply));
        if(s.pushPath)
        {
          f.push(device, s.pushPath, std::string{"mapper-write"});
          const auto bytes = frame("mapper-write", s.framingType, s.encodingType);
          take(f, *p.clientPeer, bytes);
          send(f, p.serverPeer, bytes);
          const auto reply = frame(s.constantReply.isEmpty() ? s.prefix + "mapper-write" : s.constantReply,
                                   s.framingType, s.encodingType);
          take(f, p.serverPeer, reply);
          expectValue(f, "/server_received", "mapper-write");
        }
        // Destruction cancels an incomplete frame as well as both active reads.
        send(f, p.serverPeer, startup.left(2));
        f.removeMapper(device);
        REQUIRE(f.spin([&] { return p.serverPeer.state() == QAbstractSocket::UnconnectedState
                                  && p.clientPeer->state() == QAbstractSocket::UnconnectedState; }));
      });
    }
  }
}

TEST_CASE("Mapper TCP binary size-prefix and firmware records preserve payloads", "[mapper][protocols][tcp]")
{
  for(const auto& file : {"test_tcp_size_prefix_binary.qml", "test_tcp_intel_hex_loopback.qml", "test_tcp_srec_loopback.qml"})
  {
    DYNAMIC_SECTION(file)
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto doc = score::test::new_document(ctx);
        REQUIRE(doc);
        fixture f{ctx, *doc};
        const bool ihex = QString{file}.contains("intel_hex");
        const bool srec = QString{file}.contains("srec");
        const bool records = ihex || srec;
        const auto enc = ihex ? encoding::ihex : srec ? encoding::srec : encoding::raw;
        const auto framingType = records ? framing::lf : framing::size;
        loopback p{f, f.script(file), ihex ? 5613 : srec ? 5614 : 5606};
        QByteArray startup;
        if(records)
          startup = frame(ihex ? "firmware-chunk-01" : "srec-payload-data", framingType, enc);
        else
          for(const auto& text : {"short", "a slightly longer message with more content", "x"})
            startup += frame(text, framingType);
        take(f, *p.clientPeer, startup);
        // NUL, non-UTF8, SLIP metacharacters, CR/LF: true binary, not JS text.
        const auto binary = QByteArray::fromHex("00ffc0db0d0a807f010203");
        const auto wire = records ? record(binary, srec) + "\n" : frame(binary, framingType);
        send(f, p.serverPeer, wire.left(2));
        REQUIRE_FALSE(f.spin([&] { return p.serverPeer.bytesAvailable() > 0; }, denied_ms));
        send(f, p.serverPeer, wire.mid(2));
        take(f, p.serverPeer, frame(binary, framingType, enc));
        expectValue(f, "/received_length", binary.size());
        send(f, *p.clientPeer, wire);
        expectValue(f, "/echo_length", binary.size());
        if(!records)
        {
          send(f, p.serverPeer, startup);
          take(f, p.serverPeer, startup);
          expectValue(f, "/received_length", 1);
          send(f, *p.clientPeer, startup);
          expectValue(f, "/echo_length", 1);
        }
      });
    }
  }
}

// An Intel HEX / S-record EOF record decodes to zero payload bytes: it is
// encoding metadata, so no script callback may fire for it -- neither the last
// decoded length nor the wire may change.
TEST_CASE("Mapper TCP firmware record EOF metadata is not dispatched as a message", "[mapper][protocols][tcp]")
{
  for(const auto& file : {"test_tcp_intel_hex_loopback.qml", "test_tcp_srec_loopback.qml"})
  {
    DYNAMIC_SECTION(file)
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto doc = score::test::new_document(ctx);
        REQUIRE(doc);
        fixture f{ctx, *doc};
        const bool srec = QString{file}.contains("srec");
        const auto enc = srec ? encoding::srec : encoding::ihex;
        loopback p{f, f.script(file), srec ? 5614 : 5613};
        take(f, *p.clientPeer,
             frame(srec ? "srec-payload-data" : "firmware-chunk-01", framing::lf, enc));
        const auto binary = QByteArray::fromHex("00ffc0db0d0a807f010203");
        const auto wire = record(binary, srec) + "\n";
        send(f, p.serverPeer, wire);
        take(f, p.serverPeer, frame(binary, framing::lf, enc));
        send(f, *p.clientPeer, wire);
        expectValue(f, "/received_length", binary.size());
        expectValue(f, "/echo_length", binary.size());
        const QByteArray eof = srec ? "S9030000FC\n" : ":00000001FF\n";
        send(f, p.serverPeer, eof);
        send(f, *p.clientPeer, eof);
        REQUIRE_FALSE(f.spin([&] {
          return p.serverPeer.bytesAvailable() > 0
              || value(f, "/received_length").toInt() != binary.size()
              || value(f, "/echo_length").toInt() != binary.size();
        }, denied_ms));
      });
    }
  }
}

TEST_CASE("Mapper TCP newline command dispatcher handles delimiters", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    loopback p{f, f.script("test_tcp_line_newline.qml"), 5605};
    const QByteArray startup{"PING\nSET volume 80\nSET brightness 50\n"};
    take(f, *p.clientPeer, startup);
    // Each command is acknowledged on the wire before the next one is written:
    // that acknowledgement is the proof the decoder consumed and re-armed.
    send(f, p.serverPeer, "PING\n");
    take(f, p.serverPeer, "PONG\n");
    expectValue(f, "/command_count", 1);
    send(f, p.serverPeer, "SET volume 80\n");
    take(f, p.serverPeer, "OK\n");
    expectValue(f, "/volume", "80");
    send(f, p.serverPeer, "SET brightness 50\n");
    take(f, p.serverPeer, "OK\n");
    expectValue(f, "/brightness", "50");
    expectValue(f, "/command_count", 3);
    send(f, p.serverPeer, "SET volume 2");
    REQUIRE_FALSE(f.spin([&] { return value(f, "/command_count").toInt() != 3; }, denied_ms));
    // A line split across reads is reassembled: "SET volume 25" must dispatch.
    send(f, p.serverPeer, "5\n");
    take(f, p.serverPeer, "OK\n");
    expectValue(f, "/volume", "25");
    send(f, p.serverPeer, "UNKNOWN\n");
    take(f, p.serverPeer, "ERR:unknown\n");
    send(f, p.serverPeer, "PING\n");
    take(f, p.serverPeer, "PONG\n");
    expectValue(f, "/command_count", 6);
    send(f, *p.clientPeer, "OK\n");
    expectValue(f, "/last_response", "OK");
    send(f, *p.clientPeer, "ERR:unknown\n");
    expectValue(f, "/last_response", "ERR:unknown");
    send(f, *p.clientPeer, "PONG\n");
    expectValue(f, "/last_response", "PONG");
  });
}

// A single read may carry several complete lines: each one is a message, and a
// trailing partial line waits for the read that completes it.
TEST_CASE("Mapper TCP line framing dispatches every line of a coalesced read", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    loopback p{f, f.script("test_tcp_line_newline.qml"), 5605};
    // Three complete commands plus the start of a fourth, in one write: three
    // dispatches now, and the partial command kept for the next read.
    send(f, p.serverPeer, "PING\nSET volume 80\nSET brightness 50\nSET vol");
    expectValue(f, "/command_count", 3);
    expectValue(f, "/volume", "80");
    expectValue(f, "/brightness", "50");
    take(f, p.serverPeer, "PONG\nOK\nOK\n");
    send(f, p.serverPeer, "ume 25\n");
    take(f, p.serverPeer, "OK\n");
    expectValue(f, "/volume", "25");
    expectValue(f, "/command_count", 4);
  });
}

TEST_CASE("Mapper raw TCP inbound broadcasts and observes disconnect", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    QTcpServer reservation;
    listen(reservation);
    const auto port = reservation.serverPort();
    auto qml = f.script("test_tcp_inbound.qml");
    replaceOnce(qml, "Bind: \"0.0.0.0\", Port: 5000",
                QString{"Bind: \"127.0.0.1\", Port: %1"}.arg(port));
    reservation.close();
    f.createMapper(device, qml);
    REQUIRE(f.spin([&] { return f.contents(device).contains(QString{device} + ":/broadcast"); }));
    QTcpSocket a, b;
    connect(f, a, port);
    connect(f, b, port);
    expectValue(f, "/client_count", 2);
    send(f, a, "incoming");
    expectValue(f, "/last_message", "incoming");
    f.push(device, "/broadcast", std::string{"both-peers"});
    take(f, a, "both-peers");
    take(f, b, "both-peers");
    a.disconnectFromHost();
    expectValue(f, "/client_count", 1);
    f.push(device, "/broadcast", std::string{"remaining-peer"});
    take(f, b, "remaining-peer");
    b.disconnectFromHost();
    expectValue(f, "/client_count", 0);
  });
}

TEST_CASE("Mapper raw TCP outbound and unframed loopback write real streams", "[mapper][protocols][tcp]")
{
  SECTION("test_tcp_outbound.qml")
  {
    score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
      auto doc = score::test::new_document(ctx);
      REQUIRE(doc);
      fixture f{ctx, *doc};
      QTcpServer server;
      listen(server);
      auto qml = f.script("test_tcp_outbound.qml");
      replaceOnce(qml, "Host: \"127.0.0.1\", Port: 5000",
                  QString{"Host: \"127.0.0.1\", Port: %1"}.arg(server.serverPort()));
      f.createMapper(device, qml);
      auto peer = accept(f, server);
      take(f, *peer, "hello from tcp client");
      expectValue(f, "/status", "connected");
      f.push(device, "/send", std::string{"second write"});
      take(f, *peer, "second write");
      f.removeMapper(device);
      REQUIRE(f.spin([&] { return peer->state() == QAbstractSocket::UnconnectedState; }));
    });
  }
  SECTION("test_tcp_outbound.qml sees the peer close")
  {
    score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
      auto doc = score::test::new_document(ctx);
      REQUIRE(doc);
      fixture f{ctx, *doc};
      QTcpServer server;
      listen(server);
      auto qml = f.script("test_tcp_outbound.qml");
      replaceOnce(qml, "Host: \"127.0.0.1\", Port: 5000",
                  QString{"Host: \"127.0.0.1\", Port: %1"}.arg(server.serverPort()));
      f.createMapper(device, qml);
      auto peer = accept(f, server);
      take(f, *peer, "hello from tcp client");
      expectValue(f, "/status", "connected");
      // The script declares no receive callback: the read loop exists only to
      // turn the remote EOF into the onClose the script reports here.
      peer->disconnectFromHost();
      expectValue(f, "/status", "disconnected");
      f.removeMapper(device);
    });
  }
  SECTION("test_tcp_outbound.qml reports a refused connection")
  {
    score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
      auto doc = score::test::new_document(ctx);
      REQUIRE(doc);
      fixture f{ctx, *doc};
      QTcpServer reservation;
      listen(reservation);
      const auto port = reservation.serverPort();
      auto qml = f.script("test_tcp_outbound.qml");
      replaceOnce(qml, "Host: \"127.0.0.1\", Port: 5000",
                  QString{"Host: \"127.0.0.1\", Port: %1"}.arg(port));
      // Nothing listens there anymore: the connect is refused.
      reservation.close();
      f.createMapper(device, qml);
      // The script spells its error handler onFail, the documented alias.
      expectValue(f, "/status", "failed");
      f.removeMapper(device);
    });
  }
  SECTION("test_tcp_loopback.qml")
  {
    score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
      auto doc = score::test::new_document(ctx);
      REQUIRE(doc);
      fixture f{ctx, *doc};
      loopback p{f, f.script("test_tcp_loopback.qml"), 5555};
      take(f, *p.clientPeer, "ping");
      send(f, p.serverPeer, "ping");
      take(f, p.serverPeer, "echo:ping");
      expectValue(f, "/server_received", "ping");
      expectValue(f, "/client_status", "connected");
      f.push(device, "/client_send", std::string{"next"});
      take(f, *p.clientPeer, "next");
      send(f, p.serverPeer, "next");
      take(f, p.serverPeer, "echo:next");
      expectValue(f, "/server_received", "next");
      // The client declares no receive callback either: its onClose reports the
      // EOF the always-armed read loop observes.
      p.clientPeer->disconnectFromHost();
      expectValue(f, "/client_status", "disconnected");
    });
  }
}

TEST_CASE("Mapper TCP onBytes ignores configured SLIP receive framing", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    loopback p{f, f.script("test_tcp_onbytes_backward_compat.qml"), 5604};
    const auto bytes = slip("framed-data");
    take(f, *p.clientPeer, bytes);
    send(f, p.serverPeer, bytes);
    expectValue(f, "/server_received", QString::fromUtf8(bytes));
    // No END bytes: an actual SLIP decoder would never deliver this chunk.
    send(f, *p.clientPeer, "unframed-reply");
    expectValue(f, "/client_raw_bytes", "unframed-reply");
  });
}

TEST_CASE("Mapper TCP SLIP OSC handles binary arguments and escaped payload", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    loopback p{f, f.script("test_tcp_slip_osc.qml"), 5603};
    // QML numeric arguments use the Mapper's float-valued OSC conversion.
    const auto startup = slip(osc("/sensor/temperature", ",f", be32(std::bit_cast<std::uint32_t>(22.5f))))
                       + slip(osc("/sensor/humidity", ",f", be32(std::bit_cast<std::uint32_t>(65.f))))
                       + slip(osc("/trigger"));
    take(f, *p.clientPeer, startup);
    send(f, p.serverPeer, startup);
    expectValue(f, "/osc_address", "/trigger");
    // 0x0000c0db forces both SLIP escape sequences inside a valid OSC integer.
    const auto packet = slip(osc("/binary", ",i", be32(0xc0db)));
    const auto split = packet.indexOf(char(0xdb)) + 1;
    REQUIRE(split > 0);
    send(f, p.serverPeer, packet.left(split));
    REQUIRE_FALSE(f.spin([&] { return value(f, "/osc_address") == "/binary"; }, denied_ms));
    send(f, p.serverPeer, packet.mid(split));
    expectValue(f, "/osc_address", "/binary");
    expectValue(f, "/osc_value", 49371.0);
  });
}

TEST_CASE("Mapper mixed TCP encodings remain independent", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    auto qml = f.script("test_tcp_encoding_mixed.qml");
    QTcpServer listeners[3], reservations[3];
    quint16 ports[3];
    for(int i = 0; i < 3; i++)
    {
      listen(listeners[i]);
      listen(reservations[i]);
      ports[i] = reservations[i].serverPort();
      replaceOnce(qml, QString{"Host: \"127.0.0.1\", Port: %1"}.arg(5620 + i),
                  QString{"Host: \"127.0.0.1\", Port: %1"}.arg(listeners[i].serverPort()));
      replaceOnce(qml, QString{"Bind: \"127.0.0.1\", Port: %1"}.arg(5620 + i),
                  QString{"Bind: \"127.0.0.1\", Port: %1"}.arg(ports[i]));
    }
    for(auto& reservation : reservations)
      reservation.close();
    f.createMapper(device, qml);
    const framing frames[]{framing::lf, framing::slip, framing::size};
    const encoding encodings[]{encoding::base64, encoding::hex, encoding::raw};
    const QByteArray names[]{"b64", "hex", "raw"};
    const QByteArray payloads[]{"base64 message", "hex message", "raw message"};
    std::unique_ptr<QTcpSocket> clients[3];
    QTcpSocket servers[3];
    for(int i = 0; i < 3; i++)
    {
      clients[i] = accept(f, listeners[i]);
      connect(f, servers[i], ports[i]);
      auto bytes = take(f, *clients[i], frame(payloads[i], frames[i], encodings[i]));
      send(f, servers[i], bytes);
    }
    for(int i = 0; i < 3; i++)
    {
      auto reply = take(f, servers[i], frame(names[i] + "-ok", frames[i], encodings[i]));
      send(f, *clients[i], reply);
      expectValue(f, "/" + QString::fromUtf8(names[i]) + "_received", QString::fromUtf8(payloads[i]));
      expectValue(f, "/" + QString::fromUtf8(names[i]) + "_echo", QString::fromUtf8(names[i] + "-ok"));
    }
  });
}

TEST_CASE("Mapper CasparCG AMCP commands and error responses", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_casparcg_amcp.qml", 5250};
    take(f, *p.peer, "VERSION\r\n");
    p.reply(f, "201 VERSION OK\r\n", {{"/last_response", "201 VERSION OK"}});
    p.line(f, "2.4.0\r\n");
    expectValue(f, "/last_response", "2.4.0");
    p.command(f, "/play_loop", std::string{"museum.mov"}, "PLAY 1-1 \"museum.mov\" LOOP\r\n");
    p.command(f, "/overlay_play", std::string{"title"}, "PLAY 1-2 \"title\"\r\n");
    p.command(f, "/mixer_opacity_fade", 0.5f, "MIXER 1-1 OPACITY 0.5 25 easeinsine\r\n");
    p.command(f, "/mixer_fill", std::string{"0.1 0.2 0.5 0.5"}, "MIXER 1-1 FILL 0.1 0.2 0.5 0.5\r\n");
    p.reply(f, "202 PLAY OK\r\n", {{"/last_response", "202 PLAY OK"}});
    p.line(f, "404 MIXER ERROR\r\n");
    expectValue(f, "/last_error", "404 MIXER ERROR");
    p.command(f, "/stop", 1, "STOP 1-1\r\n");
  });
}

TEST_CASE("Mapper ETC Eos OSC commands and size-prefixed console feedback", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_etc_eos_osc.qml", 3032, {}};
    expectValue(f, "/status", "ready");
    p.command(f, "/go", 1, frame(osc("/eos/key/go_0"), framing::size));
    p.command(f, "/goto_cue_list", std::string{"2 5.5"}, frame(osc("/eos/cue/2/5.5/fire"), framing::size));
    f.push(device, "/channel", 17);
    p.command(f, "/channel_level", 62.5f,
              frame(osc("/eos/chan/17", ",f", be32(std::bit_cast<std::uint32_t>(62.5f))), framing::size));
    p.command(f, "/command", std::string{"Chan 1 At 50"},
              frame(osc("/eos/newcmd", ",s", oscString("Chan 1 At 50#")), framing::size));
    const auto feedback = frame(osc("/eos/out/active/cue/2/5.5/Intro"), framing::size)
                        + frame(osc("/eos/out/pending/cue/2/6/Next"), framing::size)
                        + frame(osc("/eos/out/show/name", ",s", oscString("Local integration show")), framing::size);
    p.reply(f, feedback,
            {{"/active_cue_list", "2"},
             {"/active_cue", "5.5"},
             {"/pending_cue", "6"},
             {"/show_name", "Local integration show"}});
  });
}

TEST_CASE("Mapper Extron SIS banner, routing, mute and error feedback", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_extron_sis.qml", 23};
    p.reply(f, "Extron DXP 88 (c) 2026\r\n",
            {{"/status", "ready"}, {"/device_info", "Extron DXP 88 (c) 2026"}});
    p.command(f, "/route", std::string{"3 2"}, "3*2!\r\n");
    p.command(f, "/route_video", std::string{"4 1"}, "4*1%\r\n");
    p.command(f, "/route_audio", std::string{"5 1"}, "5*1$\r\n");
    p.command(f, "/audio_mute", 1, "1*Z\r\n");
    p.command(f, "/video_mute", 1, "1*B\r\n");
    p.command(f, "/frontpanel_lock", 1, "1X\r\n");
    p.reply(f, "In3 All Out2\r\n", {{"/last_route", "In3 All Out2"}});
    p.line(f, "Amt1\r\n");
    expectValue(f, "/audio_muted", 1);
    p.line(f, "Vmt1\r\n");
    expectValue(f, "/video_muted", 1);
    p.line(f, "Exe1\r\n");
    expectValue(f, "/frontpanel_locked", 1);
    p.line(f, "E12\r\n");
    expectValue(f, "/last_error", "E12 invalid output number");
    p.line(f, "Amt0\r\n");
    expectValue(f, "/audio_muted", 0);
    p.line(f, "Vmt0\r\n");
    expectValue(f, "/video_muted", 0);
    p.line(f, "Exe0\r\n");
    expectValue(f, "/frontpanel_locked", 0);
    p.line(f, "Password:\r\n");
    expectValue(f, "/status", "password_required");
    // Password transmission is intentionally not implemented by this script.
  });
}

TEST_CASE("Mapper grandMA telnet authenticates with device values and strips ANSI", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_grandma_telnet.qml", 30000};
    expectValue(f, "/status", "connected");
    f.push(device, "/login_user", std::string{"integration"});
    f.push(device, "/login_password", std::string{"local-only"});
    // The login prompt writes no device value at all: its only observable
    // effect is the credential line the script sends back.
    p.handshake(f, "\x1b[32mPlease login\x1b[0m\r\n", "Login integration local-only\r\n");
    p.reply(f, "Logged in as integration\r\n", {{"/status", "ready"}});
    p.command(f, "/goto_cue", 3.5f, "Goto Cue 3.5 Executor 1\r\n");
    p.command(f, "/fader_executor_2", 25.f, "Executor 2 At 25\r\n");
    p.command(f, "/lua_hardkey", 11,
              "LUA 'gma.canbus.hardkey(11, true, false)'\r\nLUA 'gma.canbus.hardkey(11, false, false)'\r\n");
    p.reply(f, "\x1b[31mExecutor 2 at 25\x1b[0m\r\n", {{"/last_response", "Executor 2 at 25"}});
    p.reply(f, "no login\r\n", {{"/status", "login_failed"}});
    // A line-delimited stream observes the peer's EOF like any other: the
    // decoder hands it to the close notification the script reports here.
    p.peer->disconnectFromHost();
    expectValue(f, "/status", "disconnected");
    // Telnet IAC negotiation and grandMA3 are outside this grandMA2 script.
  });
}

TEST_CASE("Mapper HyperDeck block greeting, commands and asynchronous notifications", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_hyperdeck_control.qml", 9993, "\n"};
    p.reply(f, "500 connection info:\r\n", {{"/status", "connected"}});
    p.line(f, "model: HyperDeck Studio\r\n");
    expectValue(f, "/model", "HyperDeck Studio");
    p.line(f, "protocol version: 1.12\r\n");
    take(f, *p.peer, "notify:\ntransport: true\nslot: true\nconfiguration: true\n\ntransport info\n\nslot info:\nslot id: 1\n\ndevice info\n\n");
    expectValue(f, "/status", "ready");
    expectValue(f, "/protocol_version", "1.12");
    p.command(f, "/record_named", std::string{"integration"}, "record:\nname: integration\n\n");
    p.command(f, "/goto_clip", 7, "goto:\nclip id: 7\n\n");
    p.command(f, "/shuttle_speed", -100, "shuttle:\nspeed: -100\n\n");
    p.block(f, {"\r\n", "200 ok\r\n", "\r\n", "508 transport info:\r\n", "status: play\r\n"},
            "/transport_state", "play");
    p.line(f, "speed: -100\r\n");
    expectValue(f, "/speed", -100);
    p.line(f, "clip id: 7\r\n");
    expectValue(f, "/current_clip", 7);
    p.line(f, "timecode: 00:00:12:04\r\n");
    expectValue(f, "/timecode", "00:00:12:04");
    p.line(f, "display timecode: 01:00:12:04\r\n");
    expectValue(f, "/display_timecode", "01:00:12:04");
    p.block(f, {"\r\n", "502 slot info:\r\n", "status: mounted\r\n"}, "/slot_status", "mounted");
    p.line(f, "recording time: 350\r\n");
    expectValue(f, "/recording_time_available", "350");
    p.block(f, {"\r\n", "204 device info:\r\n", "unique id: local-deck\r\n"}, "/unique_id",
            "local-deck");
    p.block(f, {"\r\n", "108 no disk\r\n"}, "/last_error", "108 no disk");
  });
}

TEST_CASE("Mapper Kramer Protocol 3000 routes both layers and parses machine replies", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_kramer_p3000.qml", 5000};
    take(f, *p.peer, "#MODEL?\r\n#VERSION?\r\n#INFO-IO?\r\n");
    p.reply(f, "~01@MODEL VS-88\r\n", {{"/model", "VS-88"}});
    p.line(f, "~01@VERSION 2.0\r\n");
    expectValue(f, "/firmware_version", "2.0");
    p.line(f, "~01@INFO-IO IN 8,OUT 8\r\n");
    expectValue(f, "/num_inputs", 8);
    expectValue(f, "/num_outputs", 8);
    p.command(f, "/route", std::string{"3 2"}, "#ROUTE 0,3,2\r\n#ROUTE 1,3,2\r\n");
    p.command(f, "/mute_output_1", 1, "#VMUTE 1,1\r\n");
    p.command(f, "/volume", 35, "#VOL 1,35\r\n");
    p.reply(f, "~01@ROUTE 1,3,2\r\n", {{"/last_route", "1,3,2"}});
    p.line(f, "~01@VMUTE 2,1\r\n");
    expectValue(f, "/mute_output_2", 1);
    p.line(f, "~01@VOL 1,42\r\n");
    expectValue(f, "/volume", 42);
    p.line(f, "~01@ROUTE ERR 003\r\n");
    expectValue(f, "/last_error", "ROUTE ERR 003");
  });
}

TEST_CASE("Mapper PJLink CR handshake, control and status decoding", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_pjlink_projector.qml", 4352, "\r"};
    SECTION("unauthenticated projector")
    {
      p.handshake(f, "PJLINK 0\r",
                  "%1POWR ?\r%1INPT ?\r%1AVMT ?\r%1LAMP ?\r%1ERST ?\r%1NAME ?\r%1INF1 ?\r%1INF2 ?\r");
      expectValue(f, "/status", "ready");
      p.command(f, "/power", 1, "%1POWR 1\r");
      p.command(f, "/input", 32, "%1INPT 32\r");
      p.command(f, "/mute", 1, "%1AVMT 31\r");
      p.reply(f, "%1POWR=3\r", {{"/power_state", "warming"}});
      p.line(f, "%1INPT=32\r");
      expectValue(f, "/input_state", "32");
      p.line(f, "%1AVMT=31\r");
      expectValue(f, "/mute_state", "av_mute");
      p.line(f, "%1LAMP=12345 1\r");
      expectValue(f, "/lamp_hours", 12345);
      p.line(f, "%1ERST=001000\r");
      expectValue(f, "/error_status", "001000");
      p.line(f, "%1NAME=Gallery\r");
      expectValue(f, "/projector_name", "Gallery");
      p.line(f, "%1INF1=Local Manufacturer\r");
      expectValue(f, "/manufacturer", "Local Manufacturer");
      p.line(f, "%1INF2=Local Model\r");
      expectValue(f, "/product", "Local Model");
      p.reply(f, "%1POWR=1\r", {{"/power_state", "on"}});
      p.line(f, "%1AVMT=30\r");
      expectValue(f, "/mute_state", "unmuted");
    }
    SECTION("authentication is reported, not faked")
    {
      p.reply(f, "PJLINK 1 abcdef12\r", {{"/status", "auth_required"}});
      f.push(device, "/power", 1);
      REQUIRE_FALSE(f.spin([&] { return p.peer->bytesAvailable() > 0; }, denied_ms));
      p.reply(f, "PJLINK ERRA\r", {{"/status", "auth_failed"}});
      // Challenge-response authentication is not implemented by the corpus.
    }
  });
}

TEST_CASE("Mapper Videohub state blocks and zero-indexed routing", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_videohub_router.qml", 9990, "\n"};
    // The hub dumps its whole state on connect, before any command: the first
    // lines of each block change no device value, so the block is what gets
    // acknowledged.
    p.block(f, {"PROTOCOL PREAMBLE:\r\n", "Version: 2.8\r\n"}, "/protocol_version", "2.8");
    p.block(f, {"\r\n", "VIDEOHUB DEVICE:\r\n", "Model name: Smart Videohub\r\n"}, "/model",
            "Smart Videohub");
    p.line(f, "Video inputs: 12\r\n");
    expectValue(f, "/num_inputs", 12);
    p.line(f, "Video outputs: 12\r\n");
    expectValue(f, "/num_outputs", 12);
    expectValue(f, "/status", "ready");
    p.block(f, {"\r\n", "INPUT LABELS:\r\n", "0 Camera One\r\n", "\r\n",
                "VIDEO OUTPUT ROUTING:\r\n", "0 2\r\n"},
            "/routing_0", 2);
    p.line(f, "1 3\r\n");
    expectValue(f, "/routing_1", 3);
    p.command(f, "/route", std::string{"0 5"}, "VIDEO OUTPUT ROUTING:\n0 5\n\n");
    p.command(f, "/output_3_input", 7, "VIDEO OUTPUT ROUTING:\n3 7\n\n");
    p.block(f, {"\r\n", "VIDEO OUTPUT ROUTING:\n", "0 5\n"}, "/routing_0", 5);
    p.line(f, "3 7\n");
    expectValue(f, "/routing_3", 7);
    // Labels and lock blocks are logged only; the original tree exposes no
    // nodes for them, and exposes routing feedback only for outputs 0..3.
  });
}

TEST_CASE("Mapper WATCHOUT quoted commands, dynamic inputs and playback feedback", "[mapper][protocols][tcp]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_watchout_control.qml", 3040};
    take(f, *p.peer, "ping\r\ngetStatus 1\r\n");
    p.reply(f, "Ready 12345 true\r\n",
            {{"/wo_state", "ready"}, {"/position_ms", 12345}, {"/playing", 1}});
    p.command(f, "/run_timeline", std::string{"Side wall"}, "run \"Side wall\"\r\n");
    p.command(f, "/goto_cue_and_run", std::string{"Intro"}, "gotoControlCue \"Intro\" true\r\n");
    p.command(f, "/set_input", std::string{"opacity +0.1"}, "setInput \"opacity\" +0.1\r\n");
    f.push(device, "/input_name", std::string{"museumOpacity"});
    p.command(f, "/input_value", 0.5f, "setInput \"museumOpacity\" 0.5\r\n");
    p.reply(f, "Busy 23456\r\n", {{"/wo_state", "busy"}, {"/position_ms", 23456}});
    p.line(f, "Reply 1 \"show\"\r\n");
    expectValue(f, "/last_reply", "Reply 1 \"show\"");
    p.line(f, "Status 1 23456\r\n");
    expectValue(f, "/last_status_update", "Status 1 23456");
    p.reply(f, "Error 7 \"No such timeline\"\r\n",
            {{"/wo_state", "error"}, {"/last_error", "Error 7 \"No such timeline\""}});
    p.reply(f, "Ready 23456 false\r\n", {{"/playing", 0}, {"/wo_state", "ready"}});
  });
}

TEST_CASE("Mapper TCP client reports an abortively closed peer", "[mapper][protocols][tcp]")
{
#if defined(Q_OS_UNIX)
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    hardware p{f, "test_watchout_control.qml", 3040};
    take(f, *p.peer, "ping\r\ngetStatus 1\r\n");
    expectValue(f, "/status", "ready");
    // A reset is not an orderly shutdown, and nothing else in the stack
    // observes it: the read loop has to report it exactly like an EOF.
    reset(f, *p.peer);
    expectValue(f, "/status", "disconnected");
  });
#else
  SKIP("An abortive close needs SO_LINGER on a Unix socket");
#endif
}
