// The spatialization device's source offset: source n of the device is
// transmitted as source n + offset, so that a score written against sources
// 1..N can drive a SpatGRIS / ADM-OSC / SPAT session where those sources live
// further up. The device tree keeps numbering from 1 -- the offset describes
// the far end's addressing, not the content -- so an existing document's
// addresses and cables are unaffected.
//
// Offset 0 is the wire format as it was before the setting existed. Read off
// the socket and not out of the protocol objects: an index that is right in
// the model and wrong in the packet is what this guards against.

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <Spatialization/ADMOSCProtocol.hpp>
#include <Spatialization/SPATProtocol.hpp>
#include <Spatialization/SpatGRISProtocol.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <oscpack/osc/OscReceivedElements.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{
// Own the platform runtime and socket separately from listener setup so a
// failed REQUIRE during bind or timeout configuration still releases both.
struct socket_owner
{
#if defined(_WIN32)
  static constexpr SOCKET invalid_socket = INVALID_SOCKET;
  SOCKET fd{invalid_socket};
#else
  static constexpr int invalid_socket = -1;
  int fd{invalid_socket};
#endif

  socket_owner()
  {
#if defined(_WIN32)
    WSADATA data{};
    REQUIRE(::WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  }

  ~socket_owner()
  {
#if defined(_WIN32)
    if(fd != invalid_socket)
      ::closesocket(fd);
    ::WSACleanup();
#else
    if(fd != invalid_socket)
      ::close(fd);
#endif
  }

  socket_owner(const socket_owner&) = delete;
  socket_owner& operator=(const socket_owner&) = delete;
};

//! A loopback UDP socket the protocols send to, read to exhaustion after each
//! push. Short receive timeout: a message that never comes is a failure, and
//! the test must not hang waiting for it.
struct listener : socket_owner
{
  uint16_t port{};

  listener()
  {
    REQUIRE(fd != invalid_socket);

    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    REQUIRE(::bind(fd, (sockaddr*)&a, sizeof(a)) == 0);

#if defined(_WIN32)
    int len = sizeof(a);
#else
    socklen_t len = sizeof(a);
#endif
    REQUIRE(::getsockname(fd, (sockaddr*)&a, &len) == 0);
    port = ntohs(a.sin_port);

#if defined(_WIN32)
    // Winsock expects a millisecond count, not POSIX's timeval.
    DWORD timeout = 200;
    REQUIRE(
        ::setsockopt(
            fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
            sizeof(timeout))
        == 0);
#else
    timeval tv{0, 200000};
    REQUIRE(::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0);
#endif
  }

  //! "<address> <arg> <arg>...", with floats left out: the indices and the
  //! command words are what this test is about.
  std::vector<std::string> read()
  {
    std::vector<std::string> out;
    char buf[4096];
    for(;;)
    {
      const auto n = ::recv(fd, buf, sizeof(buf), 0);
      if(n <= 0)
        return out;

      oscpack::ReceivedPacket p{buf, (std::size_t)n};
      if(!p.IsMessage())
        continue;

      oscpack::ReceivedMessage m{p};
      std::string line = m.AddressPattern();
      for(auto it = m.ArgumentsBegin(); it != m.ArgumentsEnd(); ++it)
      {
        if(it->IsString())
        {
          line += ' ';
          line += it->AsString();
        }
        else if(it->IsInt32())
        {
          line += ' ';
          line += std::to_string(it->AsInt32());
        }
      }
      out.push_back(std::move(line));
    }
  }
};

ossia::net::parameter_base* param(ossia::net::device_base& d, const char* addr)
{
  auto* n = ossia::net::find_node(d.get_root_node(), addr);
  return n ? n->get_parameter() : nullptr;
}
}

TEST_CASE("the spatialization source offset shifts the index on the wire",
          "[integration][device][spatgris]")
{
  listener sock;
  auto ctx = std::make_shared<ossia::net::network_context>();
  const ossia::net::outbound_socket_configuration out{
      .host = "127.0.0.1", .port = sock.port, .broadcast = false};

  auto spatgris = [&](int offset) {
    auto proto
        = std::make_unique<Spatialization::SpatGRISProtocol>(ctx, out, 4, 0, offset);
    ossia::net::generic_device dev{std::move(proto), "SpatGRIS"};

    REQUIRE(param(dev, "/1/position") != nullptr);
    param(dev, "/1/position")->push_value(ossia::vec3f{0.25f, 0.5f, 0.75f});
    param(dev, "/2/hspan")->push_value(0.5f);
    param(dev, "/1/clear")->push_value(ossia::impulse{});
    ctx->context.poll();
    return sock.read();
  };

  auto admosc = [&](int offset) {
    auto proto
        = std::make_unique<Spatialization::ADMOSCProtocol>(ctx, out, 4, 0, offset);
    ossia::net::generic_device dev{std::move(proto), "ADM"};

    REQUIRE(param(dev, "/adm/obj/1/azim") != nullptr);
    param(dev, "/adm/obj/1/azim")->push_value(45.f);
    param(dev, "/adm/obj/3/xyz")->push_value(ossia::vec3f{0.1f, 0.2f, 0.3f});
    param(dev, "/adm/lis/x")->push_value(0.9f);
    ctx->context.poll();
    return sock.read();
  };

  auto spat = [&](int offset) {
    auto proto = std::make_unique<Spatialization::SPATProtocol>(ctx, out, 4, 1, offset);
    ossia::net::generic_device dev{std::move(proto), "SPAT"};

    REQUIRE(param(dev, "/source/2/gain") != nullptr);
    param(dev, "/source/2/gain")->push_value(-6.f);
    param(dev, "/room/1/size")->push_value(1234.f);
    ctx->context.poll();
    return sock.read();
  };

  // Offset 0: the wire format as it was before the setting existed.
  CHECK(
      spatgris(0)
      == std::vector<std::string>{
          "/spat/serv car 1", "/spat/serv car 2", "/spat/serv clr 1"});
  CHECK(
      admosc(0)
      == std::vector<std::string>{
          "/adm/obj/1/azim", "/adm/obj/3/xyz", "/adm/lis/x"});
  CHECK(spat(0) == std::vector<std::string>{"/source/2/gain", "/room/1/size"});

  // Source 1 goes out as source 30.
  CHECK(
      spatgris(29)
      == std::vector<std::string>{
          "/spat/serv car 30", "/spat/serv car 31", "/spat/serv clr 30"});

  // The listener is not a source: it keeps its address.
  CHECK(
      admosc(29)
      == std::vector<std::string>{
          "/adm/obj/30/azim", "/adm/obj/32/xyz", "/adm/lis/x"});

  // Rooms are not sources either.
  CHECK(spat(29) == std::vector<std::string>{"/source/31/gain", "/room/1/size"});
}
