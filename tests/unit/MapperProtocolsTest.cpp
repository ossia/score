// The Mapper device driving Protocols.can() and Protocols.serial().
//
// libossia's QML tests install `Protocols` on a bare QJSEngine. In score it is a
// context property of the Mapper's engine, that engine runs on its own thread,
// and what a script produces has to come back out through the mapper's tree.
//
// Each test creates a real Mapper from a script, moves bytes on a real
// transport, and reads the result back through Score.iterateDevice().

#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <JS/Qml/EditContext.hpp>

#include <core/document/Document.hpp>

#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#if defined(__linux__)

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <cstring>
#include <string>

namespace
{
constexpr const char* can_iface = "vcan0";
constexpr const char* mapper_uuid = "910e2d87-a087-430d-b725-c988fe2bea01";

//! A raw SocketCAN peer, to put frames on the bus from the test side.
struct raw_can
{
  int fd{-1};

  raw_can()
  {
    fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(fd < 0)
      return;

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, can_iface, sizeof(ifr.ifr_name) - 1);
    if(::ioctl(fd, SIOCGIFINDEX, &ifr) != 0)
    {
      ::close(fd);
      fd = -1;
      return;
    }

    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
      ::close(fd);
      fd = -1;
    }
  }
  ~raw_can()
  {
    if(fd >= 0)
      ::close(fd);
  }

  bool valid() const { return fd >= 0; }

  void send(uint32_t id, std::initializer_list<uint8_t> bytes) const
  {
    can_frame f{};
    f.can_id = id;
    f.can_dlc = uint8_t(bytes.size());
    int i = 0;
    for(auto b : bytes)
      f.data[i++] = b;
    (void)::write(fd, &f, sizeof(f));
  }
};

//! A pty pair: the script opens the slave, the test writes on the master.
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

struct fixture
{
  const score::GUIApplicationContext& ctx;
  score::Document& doc;
  QQmlEngine engine;

  fixture(const score::GUIApplicationContext& c, score::Document& d)
      : ctx{c}
      , doc{d}
  {
    engine.globalObject().setProperty("Score", engine.newQObject(new JS::EditJsContext));
  }

  /**
   * Evaluate `js`, failing the test if it throws.
   *
   * FAIL rather than REQUIRE because this is reached from inside the polling
   * predicates: an assertion there counts once per poll, which would make the
   * test's assertion count depend on how fast the machine is. FAIL only counts
   * when it fires.
   */
  QJSValue eval(const QString& js)
  {
    auto res = engine.evaluate(js);
    if(res.isError())
      FAIL(
          "script failed: " << res.toString().toStdString()
                            << "\nscript was: " << js.toStdString());
    return res;
  }

  //! Create a Mapper device running `qml`.
  void createMapper(const QString& name, const QString& qml)
  {
    const auto settings
        = QJsonDocument{QJsonObject{{"Text", qml}}}.toJson(QJsonDocument::Compact);
    eval(QStringLiteral("Score.createDevice(\"%1\", \"%2\", %3)")
             .arg(name, mapper_uuid, QString::fromUtf8(settings)));
  }

  /**
   * Every (address, value) pair of a device, as iterateDevice yields them.
   *
   * Compiled once and reported with FAIL rather than REQUIRE: this is called
   * from inside spin()'s predicate, and an assertion there would count once per
   * poll - which makes the test's assertion count depend on how fast the
   * machine is. FAIL only counts when it fires.
   */
  QVariantMap contents(const QString& name)
  {
    if(!m_iterate.isCallable())
    {
      m_iterate = engine.evaluate(QStringLiteral(R"js(
        (function(name) {
          var res = {};
          Score.iterateDevice(name, function(addr, v) { res[addr] = v.value; });
          return res;
        })
      )js"));
      if(!m_iterate.isCallable())
        FAIL(
            "could not compile the iterateDevice wrapper: "
            << m_iterate.toString().toStdString());
    }

    auto res = m_iterate.call({name});
    if(res.isError())
      FAIL("iterateDevice failed: " << res.toString().toStdString());
    return res.toVariant().toMap();
  }

  QJSValue m_iterate;

  //! Run both loops until `pred` holds, or give up.
  template <typename F>
  bool spin(F pred, int ms = 5000)
  {
    for(int i = 0; i < ms / 5; i++)
    {
      QCoreApplication::processEvents();
      if(pred())
        return true;
      ::usleep(5000);
    }
    return pred();
  }
};
}

TEST_CASE("control: a plain mapper builds its tree", "[mapper]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper("plain", QStringLiteral(R"qml(
import Ossia 1.0 as Ossia
Ossia.Mapper {
  property int v: 7
  function createTree() {
    return [ { name: "v", type: Ossia.Type.Int, interval: 20,
               read: function() { return v; } } ];
  }
}
)qml"));
    REQUIRE(f.spin([&] { return f.contents("plain").contains("plain:/v"); }));
  });
}

TEST_CASE("a mapper script reads a CAN bus through Protocols.can", "[mapper]")
{
  raw_can peer;
  if(!peer.valid())
    SKIP("no " << can_iface << " interface available");

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};

    // The script keeps the last frame it saw and exposes it as two nodes; the
    // interval is what turns them into something iterateDevice can observe.
    f.createMapper(
        "can_mapper", QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var sock: null
  property int rpm: 0
  property int frames: 0

  // The socket is opened from createTree(), the mapper's own entry point: a
  // script that only imports Ossia has no Component attached property.
  function createTree() {
    sock = Protocols.can({
      Transport: { Interface: "%1" },
      onMessage: function(frame) {
        if(frame.id !== 0x123)
          return;
        // The payload is an ArrayBuffer, as the serial protocol's are. Checked
        // explicitly: new Uint8Array() also accepts a plain array, so reading
        // the bytes alone would not tell the two apart.
        if(!(frame.bytes instanceof ArrayBuffer))
          return;
        var b = new Uint8Array(frame.bytes);
        rpm = b[0] | (b[1] << 8);
        frames = frames + 1;
      }
    });

    return [
      { name: "rpm",    type: Ossia.Type.Int, interval: 20,
        read: function() { return rpm; } },
      { name: "frames", type: Ossia.Type.Int, interval: 20,
        read: function() { return frames; } }
    ];
  }
}
)qml")
                          .arg(QString::fromUtf8(can_iface)));

    // The tree exists before any frame has arrived.
    REQUIRE(
        f.spin([&] { return f.contents("can_mapper").contains("can_mapper:/rpm"); }));

    // 0x0BB8 == 3000, little-endian across the first two bytes.
    peer.send(0x123, {0xB8, 0x0B, 0, 0});

    const bool got = f.spin([&] {
      return f.contents("can_mapper").value("can_mapper:/rpm").toInt() == 3000;
    });
    INFO(
        "tree: "
        << QJsonDocument::fromVariant(f.contents("can_mapper")).toJson().toStdString());
    REQUIRE(got);
    REQUIRE(f.contents("can_mapper").value("can_mapper:/frames").toInt() >= 1);

    SECTION("a frame for another identifier is ignored")
    {
      const int before = f.contents("can_mapper").value("can_mapper:/frames").toInt();
      peer.send(0x456, {0xFF, 0xFF, 0, 0});
      f.spin([] { return false; }, 300);

      const auto after = f.contents("can_mapper");
      REQUIRE(after.value("can_mapper:/frames").toInt() == before);
      REQUIRE(after.value("can_mapper:/rpm").toInt() == 3000);
    }

    f.eval(QStringLiteral("Score.removeDevice(\"can_mapper\")"));
  });
}

TEST_CASE("a mapper script reads a serial port through Protocols.serial", "[mapper]")
{
  pty_pair pty;
  if(!pty.valid())
    SKIP("could not open a pty pair");

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};

    f.createMapper(
        "serial_mapper", QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  property var sock: null
  property int last: 0

  function createTree() {
    sock = Protocols.serial({
      Transport: { Port: "%1", Baud: 115200 },
      Framing: { Mode: "Line" },
      onMessage: function(txt) { last = parseInt(txt, 10); }
    });

    return [
      { name: "value", type: Ossia.Type.Int, interval: 20,
        read: function() { return last; } }
    ];
  }
}
)qml")
                             .arg(QString::fromStdString(pty.slave)));

    REQUIRE(f.spin(
        [&] { return f.contents("serial_mapper").contains("serial_mapper:/value"); }));

    const char* line = "4242\n";
    REQUIRE(::write(pty.master, line, std::strlen(line)) > 0);

    const bool got = f.spin([&] {
      return f.contents("serial_mapper").value("serial_mapper:/value").toInt() == 4242;
    });
    INFO(
        "tree: " << QJsonDocument::fromVariant(f.contents("serial_mapper"))
                        .toJson()
                        .toStdString());
    REQUIRE(got);

    f.eval(QStringLiteral("Score.removeDevice(\"serial_mapper\")"));
  });
}

#endif
