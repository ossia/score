// The Mapper device: its lifecycle, the scope of the addresses its scripts
// write to, and the Protocols.can() / Protocols.serial() bindings.
//
// libossia's QML tests install `Protocols` on a bare QJSEngine. In score it is a
// context property of the Mapper's engine, that engine runs on its own thread,
// and what a script produces has to come back out through the mapper's tree.
//
// Each test creates real Mappers from scripts, moves bytes on a real transport
// where one is involved, and reads the result back through
// Score.iterateDevice().

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
#include <score_test/Mapper.hpp>

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

//! Two mappers of the same document, both exposing a '/leaf' of their own.
//! '/poke' writes through an unqualified address, '/poke_other' through one
//! qualified with the sibling's name.
QString sharedLeafScript(const QString& other)
{
  return QStringLiteral(R"qml(
import Ossia 1.0 as Ossia

Ossia.Mapper
{
  function createTree() {
    return [
      { name: "leaf", type: Ossia.Type.Int, value: 0 },
      { name: "poke", type: Ossia.Type.Int,
        write: function(v) { Device.write("/leaf", v.value); } },
      { name: "poke_other", type: Ossia.Type.Int,
        write: function(v) { Device.write("%1:/leaf", v.value); } }
    ];
  }
}
)qml")
      .arg(other);
}

using score::test::mapper::fixture;
}

TEST_CASE("Mapper read and write callbacks survive repeated device removal", "[mapper]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    for(int cycle = 0; cycle < 8; ++cycle)
    {
      INFO("Mapper lifecycle cycle: " << cycle);
      f.createMapper("plain", QStringLiteral(R"qml(
import Ossia 1.0 as Ossia
Ossia.Mapper {
  property int v: 0
  function createTree() {
    return [
      { name: "set", type: Ossia.Type.Int,
        write: function(value) { v = value.value * 3; } },
      { name: "v", type: Ossia.Type.Int, interval: 5,
        read: function() { return v; } }
    ];
  }
}
)qml"));
      REQUIRE(f.spin([&] { return f.contents("plain").contains("plain:/v"); }));
      f.push("plain", "/set", cycle + 11);
      REQUIRE(f.spin([&] {
        return f.contents("plain").value("plain:/v").toInt() == (cycle + 11) * 3;
      }));
      // Leave queued writes and an active polling timer at destruction.
      for(int pending = 0; pending < 16; ++pending)
        f.push("plain", "/set", pending);
      f.removeMapper("plain");
      REQUIRE(f.spin([&] { return f.contents("plain").isEmpty(); }));
    }
  });
}

// A script's `Device` is its own device: an unqualified address names a node of
// that device and of no other, however many siblings of the document expose the
// same leaf name. Both directions are exercised on purpose: a resolution that
// scanned the whole document's device list would answer for both scripts out of
// whichever device came first in it, so one of the two writes below would land
// in the wrong tree whatever that order happens to be.
TEST_CASE("an unqualified mapper address resolves in the script's own device", "[mapper]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};

    f.createMapper("map_a", sharedLeafScript("map_b"));
    f.createMapper("map_b", sharedLeafScript("map_a"));

    REQUIRE(f.spin([&] {
      return f.contents("map_a").contains("map_a:/leaf")
             && f.contents("map_b").contains("map_b:/leaf");
    }));

    const auto leaf = [&](const char* dev) {
      return f.contents(dev).value(QString{dev} + ":/leaf").toInt();
    };
    // Waits for the write to land *somewhere*, so that a misrouted one is
    // observed as such instead of merely timing out.
    const auto landed = [&](int v) {
      return f.spin([&] { return leaf("map_a") == v || leaf("map_b") == v; });
    };

    REQUIRE(leaf("map_a") == 0);
    REQUIRE(leaf("map_b") == 0);

    f.push("map_a", "/poke", 11);
    REQUIRE(landed(11));
    CHECK(leaf("map_a") == 11);
    CHECK(leaf("map_b") == 0);

    f.push("map_b", "/poke", 22);
    REQUIRE(landed(22));
    CHECK(leaf("map_b") == 22);
    CHECK(leaf("map_a") == 11);

    SECTION("a qualified address still reaches the named device")
    {
      f.push("map_a", "/poke_other", 33);
      REQUIRE(landed(33));
      CHECK(leaf("map_b") == 33);
      CHECK(leaf("map_a") == 11);

      f.push("map_b", "/poke_other", 44);
      REQUIRE(landed(44));
      CHECK(leaf("map_a") == 44);
      CHECK(leaf("map_b") == 33);
    }

    f.removeMapper("map_a");
    f.removeMapper("map_b");
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
