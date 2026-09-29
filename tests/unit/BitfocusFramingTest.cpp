// The Bitfocus host's framing of the newline-delimited JSON its module sends
// over the IPC socket: messages split across reads, several in one read, larger
// than the read buffer, garbage and over-long ones; and writes to a module which
// does not read, which must neither block nor grow without bound.
// The "module" is a shell script writing to NODE_CHANNEL_FD: no node needed.
#include <Protocols/Bitfocus/BitfocusContext.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

namespace bitfocus
{
QString nodeExecutable(const QString&)
{
  return QStringLiteral("/bin/sh");
}
}

namespace
{
QCoreApplication& app()
{
  static int argc = 1;
  static char arg0[] = "test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication a{argc, argv};
  return a;
}

bool waitFor(const std::function<bool()>& pred, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!pred() && t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  return pred();
}

// printf formats: \\" is an escaped quote inside the JSON-encoded payload
constexpr auto script = R"sh(
fd=$NODE_CHANNEL_FD
var() {
  printf '{"direction":"call","name":"setVariableValues","payload":"{\\"newValues\\":[{\\"id\\":\\"%s\\",\\"value\\":%s}]}"}\n' "$1" "$2"
}

# One message over three reads
printf '{"direction":"call","name":"setVar' >&$fd
sleep 0.1
printf 'iableValues","payload":"{\\"newValues\\":[{\\"id\\":\\"a\\",' >&$fd
sleep 0.1
printf '\\"value\\":1}]}"}\n' >&$fd
sleep 0.1

# Several messages, an empty line and garbage in one write
{ var b 2; printf '\n'; printf 'not json at all\n'; printf '{"direction":\n'; var c 3; } >&$fd

# A message several times the size of the read buffer
{
  printf '{"direction":"call","name":"setVariableValues","payload":"{\\"newValues\\":[{\\"id\\":\\"d\\",\\"value\\":\\"'
  head -c 200000 /dev/zero | tr '\0' x
  printf '\\"}]}"}\n'
} >&$fd

# Over the limit the test sets: dropped, up to its end
{
  printf '{"direction":"call","name":"setVariableValues","payload":"{\\"newValues\\":[{\\"id\\":\\"lost\\",\\"value\\":\\"'
  head -c 3000000 /dev/zero | tr '\0' y
  printf '\\"}]}"}\n'
  var e 5
} >&$fd

# Never reads what the host sends
sleep 30
)sh";

struct scripted_module
{
  QTemporaryDir dir;
  std::unique_ptr<bitfocus::module_handler> handler;
  std::vector<std::pair<QString, QVariant>> received;

  scripted_module()
  {
    app();
    QFile f{dir.path() + "/module.sh"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(script);
    f.close();

    handler = std::make_unique<bitfocus::module_handler>(
        dir.path(), "module.sh", "node22", "1.14.1", bitfocus::module_configuration{});
    // Before the event loop runs: nothing has been read yet
    handler->max_message_size = 1024 * 1024;
    QObject::connect(
        handler.get(), &bitfocus::module_handler::variableChanged, handler.get(),
        [this](const QString& id, const QVariant& v) { received.emplace_back(id, v); });
  }

  ~scripted_module()
  {
    handler.reset();
    // Lets the process be reaped
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
};
}

TEST_CASE("module messages are framed whatever the reads", "[bitfocus]")
{
  if(!QFile::exists("/bin/sh"))
    SKIP("no /bin/sh");

  scripted_module m;
  REQUIRE(waitFor([&] { return m.received.size() >= 5; }));
  // Anything late would have arrived with "e"
  QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

  auto& r = m.received;
  REQUIRE(r.size() == 5);
  CHECK(r[0].first == "a");
  CHECK(r[0].second.toInt() == 1);
  CHECK(r[1].first == "b");
  CHECK(r[1].second.toInt() == 2);
  CHECK(r[2].first == "c");
  CHECK(r[2].second.toInt() == 3);
  CHECK(r[3].first == "d");
  CHECK(r[3].second.toString() == QString(200000, 'x'));
  CHECK(r[4].first == "e");
  CHECK(r[4].second.toInt() == 5);
}

TEST_CASE("writing to a module which does not read does not block", "[bitfocus]")
{
  if(!QFile::exists("/bin/sh"))
    SKIP("no /bin/sh");

  scripted_module m;
  REQUIRE(waitFor([&] { return m.received.size() >= 5; }));

  const QString payload(256 * 1024, 'z');
  QElapsedTimer t;
  t.start();
  for(int i = 0; i < 40; i++)
    m.handler->writeNotification("sharedUdpSocketMessage", payload);
  CHECK(t.elapsed() < 2000);

  // Bounded by the limit, past the socket's own buffer
  CHECK(m.handler->pending_write.size() > 0);
  CHECK(m.handler->pending_write.size() <= m.handler->max_message_size);
}
