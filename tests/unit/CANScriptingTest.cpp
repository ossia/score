/**
 * End-to-end test of the CAN device, driven entirely through score's JavaScript
 * scripting API.
 *
 * The existing CAN tests are unit-level: the DBC parser on one side, raw
 * SocketCAN frames through ossia::net::can_socket on the other. Neither ever
 * instantiates the score device, so nothing covered the part the user actually
 * gets: a device created from settings, a DBC turned into an ossia node tree, a
 * frame on the wire turned into a value at an address.
 *
 * Everything here goes through the object score's console panel binds to the
 * global name `Score` -- JS::EditJsContext -- exactly as it is installed in
 * JS::ApplicationPlugin:
 *
 *   Score.createDevice(name, uuid, settings)   creates and connects the device
 *   Score.deviceToOSCQuery(name)               the namespace it produced
 *   Score.iterateDevice(name, fn)              every (address, value) pair
 *   Score.removeDevice(name)
 *
 * The test only ever writes frames on the bus and reads the tree back through
 * those calls: no protocol internals, no direct parameter access. That is the
 * point -- the layer under test is everything between a CAN frame and what a
 * script sees, which is where the device's own bugs would live.
 *
 * The libossia QML layer was the other candidate, and is the wrong one here:
 * Protocols.can() hands a script a raw socket and builds no tree at all
 * (libossia's own tests/Qt/QmlCanTest.cpp already covers it), while
 * ossia::qt::qml_device owns the generic_device it creates and has no way to
 * adopt an existing one -- neither goes anywhere near CANDevice or the DBC, so
 * a test written against them would exercise ossia, not this protocol.
 *
 * The `Device` global of the console (ossia's qml_engine_functions, i.e.
 * Device.read/Device.write) would have given plain numbers instead of the typed
 * objects iterateDevice returns, but JS::DeviceContext carries no export macro
 * and is not linkable from here. Not worth changing the plugin's ABI for a
 * nicer-looking test.
 *
 * Skipped, not failed, when vcan0 is absent.
 */

#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <JS/Qml/EditContext.hpp>

#include <core/document/Document.hpp>

#include <QJSEngine>
#include <QQmlEngine>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_all.hpp>

#if defined(__linux__)

#include <ossia/network/sockets/can_socket.hpp>

#include <boost/asio/io_context.hpp>

#include <net/if.h>
#include <sys/ioctl.h>

#include <chrono>
#include <array>
#include <string>
#include <cstring>
#include <thread>

namespace
{
constexpr const char* can_iface = "vcan0";
constexpr const char* can_uuid = "2492941c-18ee-4f96-ac3d-c3d42c0bb649";

bool ifacePresent()
{
  int fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if(fd < 0)
    return false;

  ifreq ifr{};
  std::strncpy(ifr.ifr_name, can_iface, sizeof(ifr.ifr_name) - 1);
  const bool ok = ::ioctl(fd, SIOCGIFINDEX, &ifr) == 0;
  ::close(fd);
  return ok;
}

QString dbcPath()
{
  return QString{SCORE_CAN_TEST_DATA} + "/LPMS3_16bit.dbc";
}

/**
 * The scripting environment of the console panel, rebuilt here.
 *
 * JS::ApplicationPlugin binds the very same JS::EditJsContext to `Score` on its
 * QQmlEngine; going through the plugin instance instead would mean reaching for
 * a class that carries no export macro. The object under test is the same one.
 */
struct Console
{
  QQmlEngine engine;

  Console()
  {
    engine.globalObject().setProperty(
        "Score", engine.newQObject(new JS::EditJsContext));
  }

  QJSValue eval(const QString& js)
  {
    auto res = engine.evaluate(js);
    INFO("script: " << js.toStdString());
    INFO("result: " << res.toString().toStdString());
    REQUIRE(!res.isError());
    return res;
  }

  //! Create a CAN device. The settings keys are the ones the protocol's own
  //! JSON serialization uses, which is what createDevice() feeds them to.
  void createCanDevice(const QString& name, int nodeIdOffset = 0)
  {
    eval(QString{R"js(
      Score.createDevice("%1", "%2", {
        "Interface": "%3",
        "DBC": "%4",
        "NodeIdOffset": %5,
        "Float32Override": false,
        "FD": false,
        "FilterToDatabase": false
      })
    )js"}
             .arg(name, can_uuid, can_iface, dbcPath())
             .arg(nodeIdOffset));
  }

  //! Every address the device exposes, sorted, one per line. Returned as one
  //! std::string rather than a list: Catch2 knows how to print it, and a
  //! mismatch is then readable instead of forty {?}.
  std::string addresses(const QString& device)
  {
    return eval(QString{R"js(
      (function() {
        var out = [];
        Score.iterateDevice("%1", function(addr, value) { out.push(addr); });
        out.sort();
        return out.join("\n");
      })()
    )js"}
                    .arg(device))
        .toString()
        .toStdString();
  }

  /**
   * The value at one address, or undefined when the address is not there.
   *
   * iterateDevice hands the callback the typed form ossia's
   * js_value_outbound_visitor produces -- `{ type: Ossia.Type.Float, value: x }`
   * -- not a bare number. That is the shape a real script sees, so the test
   * unwraps it the same way one would have to.
   */
  QJSValue valueAt(const QString& device, const QString& address)
  {
    return eval(QString{R"js(
      (function() {
        var found;
        Score.iterateDevice("%1", function(addr, value) {
          if(addr === "%2")
            found = value.value;
        });
        return found;
      })()
    )js"}
                    .arg(device, address));
  }
};

//! A transmitter on the bus, independent of anything score does.
struct Bus
{
  boost::asio::io_context ctx;
  ossia::net::can_socket tx;

  Bus()
      : tx{[] {
          ossia::net::can_configuration conf;
          conf.interface_name = can_iface;
          return conf;
        }(), ctx}
  {
    tx.open();
  }

  ~Bus()
  {
    tx.close();
    ctx.poll();
  }

  // std::array, not std::initializer_list: an initializer_list returned from a
  // function dangles, and the frame then carries whatever is on the stack.
  void write(uint32_t id, const std::array<uint8_t, 8>& payload)
  {
    ossia::net::can_message m;
    m.id = id;
    m.extended = false;
    m.size = uint8_t(payload.size());
    std::memcpy(m.data, payload.data(), payload.size());
    REQUIRE(!tx.write(m));
    ctx.poll();
    ctx.restart();
  }
};

//! PDO4 at node 1 with the documented worked example:
//!   W =  9878 = 0x2696   X = -1234 = 0xFB2E   Y = 0   Z = 32767 = 0x7FFF
//! little-endian on the wire, so low byte first.
constexpr uint32_t pdo4_node1 = 0x481;
constexpr std::array<uint8_t, 8> pdo4_payload{
    0x96, 0x26, 0x2E, 0xFB, 0x00, 0x00, 0xFF, 0x7F};

/**
 * Pump both the Qt event loop and the clock until `pred` holds.
 *
 * The frame is received on the document plug-in's own asio thread, so there is
 * nothing for the test to poll -- only to wait for, while keeping Qt alive so
 * that the device's queued signals get through.
 */
template <typename F>
bool waitFor(F pred, int ms = 2000)
{
  // Polled every 10ms rather than every 1ms: `pred` here evaluates a script
  // that walks the whole device, and a 1ms period turns a two-second timeout
  // into a couple of thousand redundant Catch2 assertions.
  for(int i = 0; i < ms / 10; i++)
  {
    QApplication::processEvents();
    if(pred())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  QApplication::processEvents();
  return pred();
}
}

TEST_CASE("a CAN device created from a script exposes the DBC tree", "[can][js]")
{
  if(!ifacePresent())
  {
    SUCCEED("skipped: no vcan0 interface");
    return;
  }

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    Console js;
    js.createCanDevice("imu");
    QApplication::processEvents();

    // One address per DBC signal, named <Message>/<Signal>, under the device
    // name the script chose. The identifiers are not in the tree at all: the
    // whole point of the database is that the script sees names.
    const auto addrs = js.addresses("imu");

    const std::string expected =
        "imu:/PDO1_Transmit/Acc_CalibratedX\n"
        "imu:/PDO1_Transmit/Acc_CalibratedY\n"
        "imu:/PDO1_Transmit/Acc_CalibratedZ\n"
        "imu:/PDO1_Transmit/GyroII_Align_CalibratedX\n"
        "imu:/PDO2_Transmit/GyroII_Align_CalibratedY\n"
        "imu:/PDO2_Transmit/GyroII_Align_CalibratedZ\n"
        "imu:/PDO2_Transmit/Mag_CalibratedX\n"
        "imu:/PDO2_Transmit/Mag_CalibratedY\n"
        "imu:/PDO3_Transmit/EulerX\n"
        "imu:/PDO3_Transmit/EulerY\n"
        "imu:/PDO3_Transmit/EulerZ\n"
        "imu:/PDO3_Transmit/Mag_CalibratedZ\n"
        "imu:/PDO4_Transmit/QuaternionW\n"
        "imu:/PDO4_Transmit/QuaternionX\n"
        "imu:/PDO4_Transmit/QuaternionY\n"
        // The vendor's typo in QuatetnionZ, kept: renaming a signal would break
        // every score written against the file. It sorts last because JS orders
        // by UTF-16 code unit and 'r' < 't'.
        "imu:/PDO4_Transmit/QuatetnionZ";
    REQUIRE(addrs == expected);

    // Heatbeat (0x701) has no SG_ record at all, so it is a node with no
    // parameter: absent from iterateDevice, present in the namespace.
    const auto ns = js.eval(R"js( Score.deviceToOSCQuery("imu") )js").toString();
    REQUIRE(ns.contains("/Heatbeat"));

    // VECTOR__INDEPENDENT_SIG_MSG is Vector's holder for unused signals, not a
    // frame anything transmits; it must not become a node.
    REQUIRE(!ns.contains("VECTOR__INDEPENDENT_SIG_MSG"));

    js.eval(R"js( Score.removeDevice("imu") )js");
  });
}

TEST_CASE("a frame on the bus becomes a decoded value in the script", "[can][js]")
{
  if(!ifacePresent())
  {
    SUCCEED("skipped: no vcan0 interface");
    return;
  }

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    Console js;
    js.createCanDevice("imu");
    QApplication::processEvents();

    Bus bus;
    bus.write(pdo4_node1, pdo4_payload);

    // 9878 raw, scaled by the 0.0001 the DBC states.
    REQUIRE(waitFor([&] {
      auto v = js.valueAt("imu", "imu:/PDO4_Transmit/QuaternionW");
      return v.isNumber() && std::abs(v.toNumber() - 0.9878) < 1e-4;
    }));

    REQUIRE(
        js.valueAt("imu", "imu:/PDO4_Transmit/QuaternionX").toNumber()
        == Catch::Approx(-0.1234).margin(1e-4));
    REQUIRE(
        js.valueAt("imu", "imu:/PDO4_Transmit/QuaternionY").toNumber()
        == Catch::Approx(0.0).margin(1e-4));
    REQUIRE(
        js.valueAt("imu", "imu:/PDO4_Transmit/QuatetnionZ").toNumber()
        == Catch::Approx(3.2767).margin(1e-4));

    // Nothing else moved: a PDO4 frame carries no accelerometer.
    REQUIRE(
        js.valueAt("imu", "imu:/PDO1_Transmit/Acc_CalibratedX").toNumber()
        == Catch::Approx(0.0).margin(1e-6));

    js.eval(R"js( Score.removeDevice("imu") )js");
  });
}

TEST_CASE("a frame whose id is not in the database changes nothing", "[can][js]")
{
  if(!ifacePresent())
  {
    SUCCEED("skipped: no vcan0 interface");
    return;
  }

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    Console js;
    js.createCanDevice("imu");
    QApplication::processEvents();

    Bus bus;

    // A known frame first, so that "nothing changed" is not just "nothing ever
    // arrives": the device is demonstrably listening.
    bus.write(pdo4_node1, pdo4_payload);
    REQUIRE(waitFor([&] {
      auto v = js.valueAt("imu", "imu:/PDO4_Transmit/QuaternionW");
      return v.isNumber() && std::abs(v.toNumber() - 0.9878) < 1e-4;
    }));

    // 0x123 is in no message of this database. Its payload would decode to
    // something quite different if it were mistakenly matched.
    bus.write(0x123, {{0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F}});
    waitFor([] { return false; }, 200);

    const std::string after = js.eval(R"js(
      (function() {
        var out = [];
        Score.iterateDevice("imu", function(addr, value) { out.push(addr + "=" + value.value); });
        out.sort();
        return out.join("\n");
      })()
    )js")
                                  .toString()
                                  .toStdString();

    bus.write(0x123, {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}});
    waitFor([] { return false; }, 200);

    const std::string after2 = js.eval(R"js(
      (function() {
        var out = [];
        Score.iterateDevice("imu", function(addr, value) { out.push(addr + "=" + value.value); });
        out.sort();
        return out.join("\n");
      })()
    )js")
                                   .toString()
                                   .toStdString();

    REQUIRE(after == after2);
    REQUIRE(after.find("imu:/PDO4_Transmit/QuaternionW=0.9878") != std::string::npos);

    js.eval(R"js( Score.removeDevice("imu") )js");
  });
}

TEST_CASE("the node id offset moves the whole database to another node", "[can][js]")
{
  if(!ifacePresent())
  {
    SUCCEED("skipped: no vcan0 interface");
    return;
  }

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    Console js;

    // The chain-of-sensors case: one vendor file, two score devices, two
    // offsets. The addresses are identical -- only the identifiers they listen
    // to differ, which is exactly what makes one file serve a whole bus.
    js.createCanDevice("node1", 0);
    js.createCanDevice("node2", 1);
    QApplication::processEvents();

    auto strip = [](std::string s, const std::string& prefix) {
      for(auto p = s.find(prefix); p != std::string::npos; p = s.find(prefix))
        s.erase(p, prefix.size());
      return s;
    };
    REQUIRE(
        strip(js.addresses("node1"), "node1") == strip(js.addresses("node2"), "node2"));
    REQUIRE(!js.addresses("node1").empty());

    Bus bus;

    // 0x482 is PDO4 of the sensor at node 2: node2's shifted database knows it,
    // node1's does not.
    bus.write(0x482, pdo4_payload);
    REQUIRE(waitFor([&] {
      auto v = js.valueAt("node2", "node2:/PDO4_Transmit/QuaternionW");
      return v.isNumber() && std::abs(v.toNumber() - 0.9878) < 1e-4;
    }));
    REQUIRE(
        js.valueAt("node1", "node1:/PDO4_Transmit/QuaternionW").toNumber()
        == Catch::Approx(0.0).margin(1e-6));

    // And the other way around.
    bus.write(pdo4_node1, {{0x2E, 0xFB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}});
    REQUIRE(waitFor([&] {
      auto v = js.valueAt("node1", "node1:/PDO4_Transmit/QuaternionW");
      return v.isNumber() && std::abs(v.toNumber() + 0.1234) < 1e-4;
    }));
    REQUIRE(
        js.valueAt("node2", "node2:/PDO4_Transmit/QuaternionW").toNumber()
        == Catch::Approx(0.9878).margin(1e-4));

    js.eval(R"js( Score.removeDevice("node1") )js");
    js.eval(R"js( Score.removeDevice("node2") )js");
  });
}

#else
TEST_CASE("CAN scripting is Linux-only", "[can][js]")
{
  SUCCEED("skipped: SocketCAN is only available on Linux");
}
#endif
