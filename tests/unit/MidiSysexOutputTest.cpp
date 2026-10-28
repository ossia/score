// System-exclusive leaving a graph port for a MIDI 1 destination. A port
// carries it as SysEx7 packets, and a MIDI device output sends one packet per
// call: a MIDI 1 output has to join them into one F0 ... F7 message, as the
// LV2 host does for its plug-ins.
#include "MidiSysexTestPackets.hpp"

#include <ossia/network/context.hpp>
#include <ossia/protocols/midi/midi_device.hpp>
#include <ossia/protocols/midi/midi_protocol.hpp>

#include <libremidi/configurations.hpp>
#include <libremidi/defaults.hpp>
#include <libremidi/detail/midi_out.hpp>
#include <libremidi/detail/midi_out.hpp>
#include <libremidi/libremidi.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{
// A MIDI 1 backend that keeps every message it is asked to send.
class recording_midi1_out final : public libremidi::midi1::out_api
{
public:
  libremidi::API get_current_api() const noexcept override
  {
    return libremidi::API::DUMMY;
  }
  stdx::error open_port(const libremidi::output_port&, std::string_view) override
  {
    return {};
  }
  stdx::error close_port() override { return {}; }
  stdx::error send_message(const unsigned char* message, std::size_t size) override
  {
    sent.emplace_back(message, message + size);
    return {};
  }

  std::vector<std::vector<uint8_t>> sent;
};

std::vector<uint8_t> bytes(const libremidi::message& m)
{
  return {m.bytes.begin(), m.bytes.end()};
}
}

TEST_CASE("A MIDI 1 output joins the SysEx7 packets it is sent one by one", "[midi][sysex]")
{
  recording_midi1_out out;
  const auto mts = sysex_test::mts_message();
  const auto note = sysex_test::note_on(1, 60, 100);
  const auto packets = sysex_test::packets_of(mts);
  REQUIRE(packets.size() == 4);

  REQUIRE(out.send_ump(packets[0].data, packets[0].size()) == stdx::error{});
  REQUIRE(out.send_ump(packets[1].data, packets[1].size()) == stdx::error{});
  REQUIRE(out.send_ump(note.data, note.size()) == stdx::error{});
  REQUIRE(out.send_ump(packets[2].data, packets[2].size()) == stdx::error{});
  REQUIRE(out.send_ump(packets[3].data, packets[3].size()) == stdx::error{});

  REQUIRE(out.sent.size() == 2);
  CHECK(out.sent[0] == std::vector<uint8_t>{0x90, 60, 100});
  CHECK(out.sent[1] == mts);
}

TEST_CASE("The LV2 host's converter gives one MIDI event per sysex message", "[midi][sysex][lv2]")
{
  // As LV2/Node.hpp uses it: one convert() per packet of the port.
  libremidi::midi2_to_midi1_assembler<> conv;
  std::vector<std::vector<uint8_t>> events;
  const auto on_midi = [&](const unsigned char* midi, std::size_t n, int64_t) {
    events.emplace_back(midi, midi + n);
    return stdx::error{};
  };

  std::vector<std::vector<uint8_t>> sent;
  for(int key = 0; key < 128; key += 4)
  {
    sent.push_back(sysex_test::mts_message(uint8_t(key)));
    for(const auto& p : sysex_test::packets_of(sent.back()))
      conv.convert(p.data, p.size(), 0, on_midi);
  }
  const auto note = sysex_test::note_on(2, 64, 90);
  conv.convert(note.data, note.size(), 0, on_midi);

  REQUIRE(events.size() == 33);
  for(std::size_t i = 0; i < 32; i++)
    CHECK(events[i] == sent[i]);
  CHECK(events[32] == std::vector<uint8_t>{0x91, 64, 90});
}

TEST_CASE(
    "A MIDI device output sends a sysex message from a graph port whole, over ALSA",
    "[midi][sysex][alsa]")
{
  const auto apis = libremidi::available_apis();
  if(std::find(apis.begin(), apis.end(), libremidi::API::ALSA_SEQ) == apis.end())
    SKIP("No ALSA sequencer in this build");

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<std::vector<uint8_t>> received;

  libremidi::input_configuration in_conf;
  in_conf.ignore_sysex = false;
  in_conf.on_message = [&](libremidi::message&& m) {
    std::lock_guard lock{mutex};
    received.push_back(bytes(m));
    cv.notify_all();
  };
  libremidi::midi_in in{
      in_conf, libremidi::midi_in_configuration_for(libremidi::API::ALSA_SEQ)};
  const std::string sink = "score sysex sink";
  if(in.open_virtual_port(sink) != stdx::error{})
    SKIP("Cannot open an ALSA sequencer port");

  // A new virtual port reaches the sequencer's port list a little later.
  std::optional<libremidi::output_port> port;
  for(int attempt = 0; attempt < 20 && !port; attempt++)
  {
    // Virtual ports are listed only on request.
    libremidi::observer_configuration obs_conf;
    obs_conf.track_virtual = true;
    libremidi::observer obs{
        obs_conf, libremidi::observer_configuration_for(libremidi::API::ALSA_SEQ)};
    for(const auto& p : obs.get_output_ports())
      if(p.port_name == sink)
        port = p;
    if(!port)
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if(!port)
    SKIP("The ALSA sequencer does not list the virtual port");

  auto ctx = std::make_shared<ossia::net::network_context>();
  libremidi::output_configuration out_conf;
  ossia::net::midi::midi_device dev{
      "sysex-out", std::make_unique<ossia::net::midi::midi_protocol>(
                       ctx, ossia::net::midi::midi_protocol_configuration{"sysex-out"},
                       out_conf,
                       libremidi::midi_out_configuration_for(libremidi::API::ALSA_SEQ))};
  auto& protocol = static_cast<ossia::net::midi::midi_protocol&>(dev.get_protocol());
  REQUIRE(protocol.set_info(ossia::net::midi::midi_info{*port, false}));

  const auto mts = sysex_test::mts_message();
  for(const auto& p : sysex_test::packets_of(mts))
    protocol.push_value(p);
  protocol.push_value(sysex_test::note_on(1, 60, 100));

  std::unique_lock lock{mutex};
  cv.wait_for(lock, std::chrono::seconds(2), [&] { return received.size() >= 2; });
  REQUIRE(received.size() == 2);
  CHECK(received[0] == mts);
  CHECK(received[1] == std::vector<uint8_t>{0x90, 60, 100});
}
