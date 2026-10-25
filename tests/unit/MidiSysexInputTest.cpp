// System-exclusive received by a MIDI input device: a MIDI Time Code full
// frame (F0 7F 7F 01 01 hh mm ss ff F7) is ten bytes, two SysEx7 packets. The
// protocol has to ask its backend for it, put every packet of it in the stream
// the dataflow graph reads, and keep it away from ports bound to a channel.
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/context.hpp>
#include <ossia/protocols/midi/midi_device.hpp>
#include <ossia/protocols/midi/midi_protocol.hpp>

#include <libremidi/configurations.hpp>
#include <libremidi/defaults.hpp>
#include <libremidi/detail/conversion.hpp>
#include <libremidi/message.hpp>

#include <catch2/catch_all.hpp>

#include <vector>

namespace
{
struct midi_fixture
{
  ossia::net::network_context_ptr ctx
      = std::make_shared<ossia::net::network_context>();
  libremidi::input_configuration conf;
  ossia::net::midi::midi_device dev;

  midi_fixture()
      : dev{"midi", std::make_unique<ossia::net::midi::midi_protocol>(
                        ctx, ossia::net::midi::midi_protocol_configuration{"midi"},
                        conf, libremidi::midi_in_configuration_for(libremidi::API::KEYBOARD))}
  {
  }

  ossia::net::midi::midi_protocol& protocol() const noexcept
  {
    return static_cast<ossia::net::midi::midi_protocol&>(dev.get_protocol());
  }

  void send(std::vector<uint8_t> bytes)
  {
    libremidi::message m;
    m.bytes.assign(bytes.begin(), bytes.end());
    conf.on_message(std::move(m));
  }

  std::vector<libremidi::ump> drain()
  {
    std::vector<libremidi::ump> out;
    libremidi::ump u;
    while(protocol().messages.try_dequeue(u))
      out.push_back(u);
    return out;
  }
};

const std::vector<uint8_t> full_frame{0xF0, 0x7F, 0x7F, 0x01, 0x01, 0x21,
                                      0x02, 0x03, 0x04, 0xF7};

std::vector<uint8_t> sysex_of(std::size_t data_bytes)
{
  std::vector<uint8_t> bytes{0xF0};
  for(std::size_t i = 0; i < data_bytes; i++)
    bytes.push_back(uint8_t(i % 0x80));
  bytes.push_back(0xF7);
  return bytes;
}

std::vector<std::vector<uint8_t>> as_midi1(const std::vector<libremidi::ump>& packets)
{
  libremidi::ump_to_midi1_stream<> stream;
  std::vector<std::vector<uint8_t>> out;
  for(const auto& p : packets)
    if(auto m = stream.feed(p); !m.empty())
      out.emplace_back(m.begin(), m.end());
  return out;
}
}

TEST_CASE("A MIDI input protocol asks its backend for system-exclusive", "[midi][sysex]")
{
  midi_fixture f;
  CHECK(!f.conf.ignore_sysex);
}

TEST_CASE("A full frame reaches the MIDI stream as its two SysEx7 packets", "[midi][sysex]")
{
  midi_fixture f;
  f.protocol().enable_registration();
  f.send(full_frame);

  const auto packets = f.drain();
  REQUIRE(packets.size() == 2);
  CHECK(packets[0].data[0] == 0x30167F7F); // start, 6 bytes: 7F 7F 01 01 21 02
  CHECK(packets[0].data[1] == 0x01012102);
  CHECK(packets[1].data[0] == 0x30320304); // end, 2 bytes: 03 04
  CHECK(as_midi1(packets) == std::vector<std::vector<uint8_t>>{full_frame});
}

TEST_CASE("System-exclusive of any length reaches the MIDI stream whole", "[midi][sysex]")
{
  midi_fixture f;
  f.protocol().enable_registration();
  for(std::size_t n : {0, 3, 6, 7, 100, 255, 256, 300, 1000})
  {
    INFO(n << " data bytes");
    f.send(sysex_of(n));
    const auto packets = f.drain();
    CHECK(packets.size() == std::max<std::size_t>(1, (n + 5) / 6));
    CHECK(as_midi1(packets) == std::vector<std::vector<uint8_t>>{sysex_of(n)});
  }
}

TEST_CASE(
    "A port on the device gets the full frame, a port on a channel does not",
    "[midi][sysex]")
{
  midi_fixture f;
  f.dev.create_full_tree();
  auto* channel_7 = ossia::net::find_node(f.dev, "/7");
  REQUIRE(channel_7);

  ossia::execution_state state;
  state.register_device(&f.dev);
  ossia::midi_inlet device_port, channel_port;
  device_port.address = &f.dev.get_root_node();
  channel_port.address = channel_7;
  state.register_port(device_port);
  state.register_port(channel_port);
  state.begin_tick();

  f.send(full_frame);
  f.send({0x96, 60, 100});
  state.begin_tick();
  state.copy_from_global_node(f.dev.get_root_node(), device_port);
  state.copy_from_global_node(*channel_7, channel_port);

  const auto& on_device = device_port.data.messages;
  REQUIRE(on_device.size() == 3);
  CHECK(as_midi1({on_device.begin(), on_device.end()})
        == std::vector<std::vector<uint8_t>>{full_frame, {0x96, 60, 100}});

  // The start packet holds 6 bytes, in the bits that are the channel of a
  // channel message: 7. Channel 7 hears the note and nothing else.
  const auto& on_channel = channel_port.data.messages;
  REQUIRE(on_channel.size() == 1);
  CHECK(as_midi1({on_channel.begin(), on_channel.end()})
        == std::vector<std::vector<uint8_t>>{{0x96, 60, 100}});

  state.unregister_port(device_port);
  state.unregister_port(channel_port);
  state.unregister_device(&f.dev);
  state.begin_tick();
}
