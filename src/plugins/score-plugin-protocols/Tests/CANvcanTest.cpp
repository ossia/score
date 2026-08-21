/**
 * End-to-end test of the CAN device's decoding path over a real SocketCAN
 * interface: a frame is written on `vcan0`, received by a second socket bound
 * to the same interface, dispatched by (id, extended) through the same kind of
 * table the protocol builds, and decoded through the DBC.
 *
 * This is deliberately *not* the score protocol object: instantiating that
 * needs a document and a network context, which would turn a decoding test into
 * an application test. What is exercised here is everything between the wire
 * and the value -- which is where a byte-order or an identifier-masking mistake
 * would actually show up.
 *
 * Skipped, not failed, when vcan0 is absent: CI machines generally do not have
 * the vcan module loaded.
 */

#include <Protocols/CAN/DBCParser.hpp>

#include <ossia/network/sockets/can_socket.hpp>

#include <catch2/catch_all.hpp>

#if defined(__linux__)
#include <boost/asio/io_context.hpp>

#include <net/if.h>
#include <sys/ioctl.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace Protocols::CAN;

namespace
{
constexpr const char* can_iface = "vcan0";

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

//! Run the io_context until `pred` holds or we give up.
template <typename F>
bool spin(boost::asio::io_context& ctx, F pred, int ms = 1000)
{
  for(int i = 0; i < ms; i++)
  {
    ctx.poll();
    ctx.restart();
    if(pred())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return pred();
}

std::string dataPath(const char* name)
{
  return std::string{SCORE_CAN_TEST_DATA} + "/" + name;
}

constexpr uint64_t frameKey(uint32_t id, bool extended) noexcept
{
  return (uint64_t(extended) << 32) | uint64_t(id);
}
}

TEST_CASE("A DBC-described frame decodes off a real CAN bus", "[can][vcan]")
{
  if(!ifacePresent())
  {
    SUCCEED("skipped: no vcan0 interface (modprobe vcan; ip link add dev vcan0 "
            "type vcan; ip link set up vcan0)");
    return;
  }

  auto db = parseDBCFile(dataPath("LPMS3_16bit.dbc"));
  REQUIRE(db.messages.size() == 5);

  // The same dispatch table the protocol builds: keyed on the (id, extended)
  // pair, built once.
  std::unordered_map<uint64_t, const Message*> byId;
  for(const auto& m : db.messages)
    byId.emplace(frameKey(m.id, m.extended), &m);

  boost::asio::io_context ctx;

  ossia::net::can_configuration conf;
  conf.interface_name = can_iface;

  ossia::net::can_socket rx{conf, ctx};
  ossia::net::can_socket tx{conf, ctx};
  rx.open();
  tx.open();

  std::vector<ossia::net::can_message> received;
  rx.receive([&](const ossia::net::can_message& m) { received.push_back(m); });

  // PDO4 at node 1, carrying the documented worked example in W.
  //   W =  9878 = 0x2696 -> 96 26      X = -1234 = 0xFB2E -> 2E FB
  //   Y =     0          -> 00 00      Z = 32767 = 0x7FFF -> FF 7F
  ossia::net::can_message out;
  out.id = 0x481;
  out.extended = false;
  out.size = 8;
  const uint8_t payload[8] = {0x96, 0x26, 0x2E, 0xFB, 0x00, 0x00, 0xFF, 0x7F};
  std::memcpy(out.data, payload, 8);

  // A frame that is not in the database, to check it is ignored rather than
  // decoded as something else.
  ossia::net::can_message noise;
  noise.id = 0x123;
  noise.extended = false;
  noise.size = 8;

  REQUIRE(!tx.write(out));
  REQUIRE(!tx.write(noise));

  REQUIRE(spin(ctx, [&] { return received.size() >= 2; }));

  int decoded = 0;
  int ignored = 0;
  for(const auto& msg : received)
  {
    const auto it = byId.find(frameKey(msg.id, msg.extended));
    if(it == byId.end())
    {
      ++ignored;
      continue;
    }

    const Message& m = *it->second;
    REQUIRE(m.name == "PDO4_Transmit");

    REQUIRE(decodeSignal(*m.findSignal("QuaternionW"), msg.data, msg.size)
            == Catch::Approx(0.9878));
    REQUIRE(decodeSignal(*m.findSignal("QuaternionX"), msg.data, msg.size)
            == Catch::Approx(-0.1234));
    REQUIRE(decodeSignal(*m.findSignal("QuaternionY"), msg.data, msg.size)
            == Catch::Approx(0.0));
    // The vendor's typo, preserved.
    REQUIRE(decodeSignal(*m.findSignal("QuatetnionZ"), msg.data, msg.size)
            == Catch::Approx(3.2767));
    ++decoded;
  }

  REQUIRE(decoded == 1);
  REQUIRE(ignored == 1);

  rx.close();
  tx.close();
  ctx.poll();
}

TEST_CASE("The node id offset picks a different device off the same bus", "[can][vcan]")
{
  if(!ifacePresent())
  {
    SUCCEED("skipped: no vcan0 interface");
    return;
  }

  // Two databases from one file: node 1 and node 2. This is the chain-of-
  // sensors case -- one vendor DBC, N score devices, N offsets.
  auto node1 = parseDBCFile(dataPath("LPMS3_16bit.dbc"));
  auto node2 = parseDBCFile(dataPath("LPMS3_16bit.dbc"));
  applyNodeIdOffset(node2, 1);

  REQUIRE(node1.findMessage(0x481, false) != nullptr);
  REQUIRE(node2.findMessage(0x482, false) != nullptr);
  REQUIRE(node2.findMessage(0x481, false) == nullptr);

  boost::asio::io_context ctx;
  ossia::net::can_configuration conf;
  conf.interface_name = can_iface;

  ossia::net::can_socket rx{conf, ctx};
  ossia::net::can_socket tx{conf, ctx};
  rx.open();
  tx.open();

  std::vector<ossia::net::can_message> received;
  rx.receive([&](const ossia::net::can_message& m) { received.push_back(m); });

  // The second sensor's PDO4.
  ossia::net::can_message out;
  out.id = 0x482;
  out.size = 8;
  const uint8_t payload[8] = {0x96, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  std::memcpy(out.data, payload, 8);
  REQUIRE(!tx.write(out));

  REQUIRE(spin(ctx, [&] { return !received.empty(); }));

  const auto& msg = received.front();
  REQUIRE(msg.id == 0x482);

  // The node-1 database does not know this frame; the node-2 one does.
  REQUIRE(node1.findMessage(msg.id, msg.extended) == nullptr);

  const auto* m = node2.findMessage(msg.id, msg.extended);
  REQUIRE(m != nullptr);
  REQUIRE(m->name == "PDO4_Transmit");
  REQUIRE(decodeSignal(*m->findSignal("QuaternionW"), msg.data, msg.size)
          == Catch::Approx(0.9878));

  rx.close();
  tx.close();
  ctx.poll();
}

#else
TEST_CASE("CAN over vcan is Linux-only", "[can][vcan]")
{
  SUCCEED("skipped: SocketCAN is only available on Linux");
}
#endif
