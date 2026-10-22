#include <Advanced/MidiScaler/MidiCCSnapshot.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using S = mtk::MidiCCSnapshot;

namespace
{
using bytes = std::vector<int>;

libremidi::message msg(std::initializer_list<unsigned char> b, int ts = 0)
{
  libremidi::message m{b};
  m.timestamp = ts;
  return m;
}

struct driver
{
  S fx;

  void in(const libremidi::message& m) { fx.inputs.midi.midi_messages.push_back(m); }

  std::vector<bytes> run(bool send = false, int frames = 64)
  {
    if(send)
      fx.inputs.send.value = halp::impulse_type{};
    fx.outputs.midi.midi_messages.clear();
    fx(halp::tick_musical{.frames = frames});
    fx.inputs.send.value.reset();
    std::vector<bytes> out;
    for(auto& m : fx.outputs.midi)
    {
      CHECK(m.timestamp >= 0);
      CHECK((frames == 0 ? m.timestamp == 0 : m.timestamp < frames));
      out.push_back(bytes(m.bytes.begin(), m.bytes.end()));
    }
    fx.inputs.midi.midi_messages.clear();
    return out;
  }
};
}

TEST_CASE("Midi CC snapshot sends its slots on Send", "[midi_cc_snapshot]")
{
  driver d;
  d.fx.inputs.count.value = 3;
  d.fx.inputs.channel.value = 2;
  d.fx.inputs.value1.value = 10;
  d.fx.inputs.cc2.value = 7;
  d.fx.inputs.value2.value = 100;
  d.in(msg({0x90, 60, 100}, 5));

  // Nothing without the trigger; MIDI passes through
  CHECK(d.run() == std::vector<bytes>{{0x90, 60, 100}});

  d.in(msg({0x80, 60, 0}, 5));
  CHECK(
      d.run(true)
      == std::vector<bytes>{{0xB1, 20, 10}, {0xB1, 7, 100}, {0xB1, 22, 0}, {0x80, 60, 0}});
  CHECK(d.run().empty());
}

TEST_CASE("Midi CC snapshot sends on change", "[midi_cc_snapshot]")
{
  driver d;
  d.fx.inputs.count.value = 2;
  d.run();
  d.fx.inputs.value1.value = 50;
  CHECK(d.run().empty());

  d.fx.inputs.on_change.value = true;
  d.fx.inputs.value2.value = 60;
  CHECK(d.run() == std::vector<bytes>{{0xB0, 21, 60}});
  // Slots beyond the count are not sent
  d.fx.inputs.value3.value = 60;
  CHECK(d.run().empty());
  CHECK(d.run().empty());
}

TEST_CASE("Midi CC snapshot takeover", "[midi_cc_snapshot]")
{
  driver d;
  d.fx.inputs.count.value = 1;
  d.fx.inputs.mode.value = S::Takeover;
  d.fx.inputs.value1.value = 64;

  // Before the slot is sent, incoming values pass
  d.in(msg({0xB0, 20, 10}));
  CHECK(d.run() == std::vector<bytes>{{0xB0, 20, 10}});

  CHECK(d.run(true) == std::vector<bytes>{{0xB0, 20, 64}});

  // Held back below the stored value, other controllers and channels pass
  d.in(msg({0xB0, 20, 10}));
  d.in(msg({0xB0, 20, 40}));
  d.in(msg({0xB0, 21, 40}));
  d.in(msg({0xB1, 20, 40}));
  CHECK(d.run() == std::vector<bytes>{{0xB0, 21, 40}, {0xB1, 20, 40}});

  // Crossing releases it, including the crossing value
  d.in(msg({0xB0, 20, 70}));
  d.in(msg({0xB0, 20, 30}));
  CHECK(d.run() == std::vector<bytes>{{0xB0, 20, 70}, {0xB0, 20, 30}});

  SECTION("Reaching the value exactly releases it")
  {
    d.run(true);
    d.in(msg({0xB0, 20, 64}));
    CHECK(d.run() == std::vector<bytes>{{0xB0, 20, 64}});
  }

  SECTION("Stop releases the locks")
  {
    d.run(true);
    d.fx.stop();
    d.in(msg({0xB0, 20, 1}));
    CHECK(d.run(false, 0) == std::vector<bytes>{{0xB0, 20, 1}});
  }

  SECTION("Immediate mode never holds back")
  {
    d.fx.inputs.mode.value = S::Immediate;
    d.run(true);
    d.in(msg({0xB0, 20, 1}));
    CHECK(d.run() == std::vector<bytes>{{0xB0, 20, 1}});
  }
}

TEST_CASE("Midi CC snapshot passes notes and all notes off", "[midi_cc_snapshot]")
{
  driver d;
  d.fx.inputs.mode.value = S::Takeover;
  d.in(msg({0x90, 60, 100}));
  d.in(msg({0xB0, 123, 0}));
  d.in(msg({0xF8}));
  d.in(msg({0x90, 60, 0}));
  CHECK(
      d.run() == std::vector<bytes>{{0x90, 60, 100}, {0xB0, 123, 0}, {0xF8}, {0x90, 60, 0}});
}
