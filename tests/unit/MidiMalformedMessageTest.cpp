// A MIDI input protocol indexes fixed 128-element tables with the data bytes of
// the messages it receives. Those bytes come from the outside world: a hardware
// interface, a MIDI-over-IP peer, a software backend such as libremidi's
// computer-keyboard input, or a stream that is simply truncated. None of them
// are trusted, so a note, controller or program number that is not a valid
// 7-bit index, and a message that is shorter than its status byte implies, must
// be dropped rather than indexed with.
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/context.hpp>
#include <ossia/protocols/midi/midi_device.hpp>
#include <ossia/protocols/midi/midi_protocol.hpp>

#include <libremidi/configurations.hpp>
#include <libremidi/defaults.hpp>
#include <libremidi/message.hpp>

#include <catch2/catch_all.hpp>

namespace
{
struct midi_fixture
{
  ossia::net::network_context_ptr ctx
      = std::make_shared<ossia::net::network_context>();
  libremidi::input_configuration conf;
  ossia::net::midi::midi_device dev;

  explicit midi_fixture(bool velocity_zero_is_note_off = false)
      : dev{"midi", std::make_unique<ossia::net::midi::midi_protocol>(
                        ctx,
                        ossia::net::midi::midi_protocol_configuration{
                            "midi", velocity_zero_is_note_off},
                        conf,
                        libremidi::midi_in_configuration_for(
                            libremidi::API::KEYBOARD))}
  {
  }

  ossia::net::midi::midi_protocol& protocol() const noexcept
  {
    return static_cast<ossia::net::midi::midi_protocol&>(dev.get_protocol());
  }

  void send(libremidi::message m) { conf.on_message(std::move(m)); }

  ossia::value value_at(std::string_view addr)
  {
    auto n = ossia::net::find_node(dev, addr);
    if(!n)
      return {};
    auto p = n->get_parameter();
    return p ? p->value() : ossia::value{};
  }
};

constexpr uint8_t note_on_1 = 0x90;
constexpr uint8_t note_off_1 = 0x80;
constexpr uint8_t control_change_1 = 0xB0;
constexpr uint8_t program_change_1 = 0xC0;
constexpr uint8_t pitch_bend_1 = 0xE0;
}

TEST_CASE("Learning only creates nodes for valid data bytes", "[midi]")
{
  midi_fixture f;
  f.protocol().set_learning(true);

  f.send({note_on_1, 60, 100});
  REQUIRE(ossia::net::find_node(f.dev, "/1/on/60") != nullptr);

  f.send({note_off_1, 61, 0});
  REQUIRE(ossia::net::find_node(f.dev, "/1/off/61") != nullptr);

  f.send({control_change_1, 7, 64});
  REQUIRE(ossia::net::find_node(f.dev, "/1/control/7") != nullptr);

  f.send({program_change_1, 12});
  REQUIRE(ossia::net::find_node(f.dev, "/1/program/12") != nullptr);

  // Data bytes above 127 do not index the parameter tables of the channel.
  f.send({note_on_1, 200, 100});
  f.send({note_off_1, 201, 0});
  f.send({control_change_1, 202, 64});
  f.send({program_change_1, 203});

  CHECK(ossia::net::find_node(f.dev, "/1/on/200") == nullptr);
  CHECK(ossia::net::find_node(f.dev, "/1/off/201") == nullptr);
  CHECK(ossia::net::find_node(f.dev, "/1/control/202") == nullptr);
  CHECK(ossia::net::find_node(f.dev, "/1/program/203") == nullptr);

  // Truncated messages do not read past their own bytes.
  f.send({note_on_1, 62});
  f.send({control_change_1, 8});
  f.send({program_change_1});
  f.send({pitch_bend_1, 0});
  f.send({});

  CHECK(ossia::net::find_node(f.dev, "/1/on/62") == nullptr);
  CHECK(ossia::net::find_node(f.dev, "/1/control/8") == nullptr);
}

TEST_CASE("Out-of-range data bytes do not reach the channel tables", "[midi]")
{
  midi_fixture f;
  auto& proto = f.protocol();

  proto.set_learning(true);
  f.send({note_on_1, 60, 100});
  f.send({note_off_1, 60, 0});
  f.send({control_change_1, 7, 10});
  proto.set_learning(false);

  f.send({note_on_1, 60, 42});
  CHECK(f.value_at("/1/on/60") == ossia::value{42});

  f.send({note_on_1, 200, 100});
  f.send({note_off_1, 200, 0});
  f.send({control_change_1, 200, 64});
  f.send({program_change_1, 200});
  f.send({0x90 | 0x0F, 255, 255});

  // A rejected message leaves the last valid one in place.
  CHECK(f.value_at("/1/on/60") == ossia::value{42});

  f.send({note_on_1, 61, 43});
  CHECK(f.value_at("/1/on/60") == ossia::value{42});
}

TEST_CASE("Truncated messages do not read past their own bytes", "[midi]")
{
  midi_fixture f;

  f.send({});
  f.send({note_on_1});
  f.send({note_on_1, 60});
  f.send({note_off_1, 60});
  f.send({control_change_1, 7});
  f.send({program_change_1});
  f.send({pitch_bend_1});
  f.send({pitch_bend_1, 0});

  SUCCEED();
}

TEST_CASE("A truncated note-on survives the velocity-zero rewrite", "[midi]")
{
  midi_fixture f{true};

  f.send({note_on_1, 60});
  f.send({note_on_1});

  f.protocol().set_learning(true);
  f.send({note_on_1, 60, 0});
  CHECK(ossia::net::find_node(f.dev, "/1/off/60") != nullptr);
}
