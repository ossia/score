// The MIDI Sync In presets of the default package load onto the object,
// decode the clock or timecode they are named for, and pass the clock on
// divided, shifted or delayed as they say.

#include "MidiPresetHarness.hpp"

#include <Midi/MidiSyncInput.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <map>
#include <vector>

using Catch::Approx;
using Sync = mo::MIDISyncIn;

namespace
{
constexpr double rate = 48000.;

struct event
{
  int64_t at{};
  std::vector<uint8_t> bytes;
};

struct driver
{
  Sync fx;
  int64_t now{};
  std::vector<event> in, out;

  driver()
  {
    fx.prepare(halp::setup{
        .input_channels = 0, .output_channels = 0, .frames = 512, .rate = rate});
  }

  void send(std::initializer_list<uint8_t> bytes, int ts = 0)
  {
    libremidi::message m;
    m.bytes.assign(bytes.begin(), bytes.end());
    m.timestamp = ts;
    fx.inputs.midi_in.push_back(m);
    in.push_back(event{now + ts, {bytes.begin(), bytes.end()}});
  }

  void tick(int frames = 512)
  {
    fx.outputs.clock_out.midi_messages.clear();
    fx(frames);
    for(const auto& m : fx.outputs.clock_out.midi_messages)
    {
      REQUIRE(m.timestamp >= 0);
      REQUIRE(m.timestamp < frames);
      out.push_back(event{now + m.timestamp, {m.bytes.begin(), m.bytes.end()}});
    }
    fx.inputs.midi_in.midi_messages.clear();
    now += frames;
  }

  //! Start, then `clocks` clocks, one per tick at timestamp `ts`.
  void start_and_clock(int clocks, int ts = 100)
  {
    send({0xFA}, 0);
    tick();
    for(int i = 0; i < clocks; i++)
    {
      send({0xF8}, ts);
      tick();
    }
  }

  //! The eight quarter frames of a time code.
  void quarter_frames(int h, int m, int s, int f, int code)
  {
    const uint8_t pieces[8]
        = {uint8_t(f & 0x0F),
           uint8_t((f >> 4) & 0x01),
           uint8_t(s & 0x0F),
           uint8_t((s >> 4) & 0x03),
           uint8_t(m & 0x0F),
           uint8_t((m >> 4) & 0x03),
           uint8_t(h & 0x0F),
           uint8_t(((h >> 4) & 0x01) | (code << 1))};
    for(int i = 0; i < 8; i++)
    {
      send({0xF1, uint8_t((i << 4) | pieces[i])});
      tick();
    }
  }

  int count(uint8_t status) const
  {
    int n = 0;
    for(auto& e : out)
      n += (!e.bytes.empty() && e.bytes[0] == status);
    return n;
  }

  std::vector<int64_t> dates(const std::vector<event>& v, uint8_t status) const
  {
    std::vector<int64_t> d;
    for(auto& e : v)
      if(!e.bytes.empty() && e.bytes[0] == status)
        d.push_back(e.at);
    return d;
  }
};

std::map<QString, midi_presets::preset> load_all()
{
  std::map<QString, midi_presets::preset> res;
  for(auto& p :
      midi_presets::load("MIDI Sync In", "f8c7e2a3-9b4d-4e5f-a1c2-3d4e5f6a7b8c"))
    res[p.name] = p;
  return res;
}

void apply(driver& d, const std::map<QString, midi_presets::preset>& all, const char* name)
{
  auto it = all.find(QString::fromUtf8(name));
  INFO(name);
  REQUIRE(it != all.end());
  const auto errors = midi_presets::apply(d.fx.inputs, it->second);
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
}
}

TEST_CASE("MIDI Sync In presets load", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  REQUIRE(all.size() >= 9);
  for(auto& [name, p] : all)
  {
    INFO(name.toStdString());
    driver d;
    const auto errors = midi_presets::apply(d.fx.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
    d.start_and_clock(24);
    d.send({0xFC});
    d.tick();
    // Start and Stop always go through.
    CHECK(d.count(0xFA) == 1);
    CHECK(d.count(0xFC) == 1);
  }
}

TEST_CASE("MIDI Sync In preset: follow MIDI clock", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Follow MIDI clock");
  // Two beats at the base tempo of 120 BPM: one second.
  d.start_and_clock(48);
  CHECK(d.fx.outputs.clock_active.value);
  CHECK(d.fx.outputs.valid.value);
  CHECK(d.fx.outputs.timecode.value == Approx(1.0).margin(1e-6));

  // Song position 16 sixteenths: four beats, two seconds.
  d.send({0xF2, 16, 0});
  d.tick();
  CHECK(d.fx.outputs.song_position.value == 16);
  CHECK(d.fx.outputs.timecode.value == Approx(2.0).margin(1e-6));

  // Everything passes untouched.
  CHECK(d.count(0xF8) == 48);
  CHECK(d.count(0xF2) == 1);
  CHECK(d.dates(d.out, 0xF8) == d.dates(d.in, 0xF8));

  d.send({0xFC});
  d.tick();
  CHECK_FALSE(d.fx.outputs.valid.value);
}

TEST_CASE("MIDI Sync In preset: follow MTC 25 fps", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Follow MTC 25 fps");
  d.quarter_frames(0, 1, 2, 10, 0b01);
  CHECK(d.fx.outputs.mtc_active.value);
  CHECK(d.fx.outputs.valid.value);
  CHECK(d.fx.outputs.frame_rate.value == Approx(25.));
  CHECK(d.fx.outputs.timecode.value == Approx(62.4).margin(1e-6));

  // Clocks do not move an MTC-only position.
  for(int i = 0; i < 24; i++)
  {
    d.send({0xF8});
    d.tick();
  }
  CHECK(d.fx.outputs.timecode.value == Approx(62.4).margin(1e-6));
}

TEST_CASE("MIDI Sync In preset: follow MTC from a full frame", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Follow MTC 25 fps");
  // Full frame 00:01:02:10 at 25 fps.
  d.send({0xF0, 0x7F, 0x7F, 0x01, 0x01, uint8_t(0b01 << 5), 1, 2, 10, 0xF7});
  d.tick();
  CHECK(d.fx.outputs.valid.value);
  CHECK(d.fx.outputs.timecode.value == Approx(62.4).margin(1e-6));
}

TEST_CASE("MIDI Sync In preset: MTC with clock interpolation", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "MTC with clock interpolation");
  d.quarter_frames(0, 0, 10, 0, 0b01);
  CHECK(d.fx.outputs.frame_rate.value == Approx(25.));
  CHECK(d.fx.outputs.timecode.value == Approx(10.).margin(1e-6));
  // Half a beat of clocks at 120 BPM: a quarter of a second more.
  for(int i = 0; i < 12; i++)
  {
    d.send({0xF8});
    d.tick();
  }
  CHECK(d.fx.outputs.clock_active.value);
  CHECK(d.fx.outputs.timecode.value == Approx(10.25).margin(1e-6));
}

TEST_CASE("MIDI Sync In preset: timecode in milliseconds", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Timecode in milliseconds");
  d.quarter_frames(0, 0, 10, 0, 0b01);
  CHECK(d.fx.outputs.timecode.value == Approx(10000.).margin(1e-3));
  for(int i = 0; i < 12; i++)
  {
    d.send({0xF8});
    d.tick();
  }
  CHECK(d.fx.outputs.timecode.value == Approx(10250.).margin(1e-3));
}

TEST_CASE("MIDI Sync In preset: MTC at 30 fps decodes as 30 fps", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "MTC with clock interpolation");
  // Rate code 3 is 30 fps, code 2 is 29.97 drop frame.
  d.quarter_frames(0, 0, 10, 15, 0b11);
  CHECK(d.fx.outputs.frame_rate.value == Approx(30.));
  CHECK(d.fx.outputs.timecode.value == Approx(10.5).margin(1e-6));
}

TEST_CASE("MIDI Sync In preset: clock dividers", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  for(auto [name, n] : {std::pair{"Clock divided by 2 for half time gear", 2},
                        std::pair{"Clock divided by 4", 4}})
  {
    INFO(name);
    driver d;
    apply(d, all, name);
    d.start_and_clock(48);
    CHECK(d.count(0xFA) == 1);
    CHECK(d.count(0xF8) == 48 / n);
    // The first clock after Start passes, then one in n.
    const auto in = d.dates(d.in, 0xF8);
    const auto out = d.dates(d.out, 0xF8);
    REQUIRE(out.size() == in.size() / n);
    for(std::size_t i = 0; i < out.size(); i++)
      CHECK(out[i] == in[i * n]);
    // Decoding still sees every clock.
    CHECK(d.fx.outputs.timecode.value == Approx(1.0).margin(1e-6));
  }
}

TEST_CASE("MIDI Sync In preset: clock delayed by 10 ms", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Clock delayed by 10 ms");
  d.start_and_clock(24);
  d.tick();
  const int64_t delay = int64_t(0.010 * rate);
  const auto in = d.dates(d.in, 0xF8);
  const auto out = d.dates(d.out, 0xF8);
  REQUIRE(out.size() == in.size());
  for(std::size_t i = 0; i < out.size(); i++)
    CHECK(out[i] - in[i] == delay);
  const auto s_in = d.dates(d.in, 0xFA);
  const auto s_out = d.dates(d.out, 0xFA);
  REQUIRE(s_out.size() == 1);
  CHECK(s_out[0] - s_in[0] == delay);
}

TEST_CASE("MIDI Sync In preset: start shifted by a sixteenth", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Start shifted by a sixteenth");
  d.start_and_clock(24);
  // The six clocks of the first sixteenth are dropped.
  CHECK(d.count(0xF8) == 18);
  const auto in = d.dates(d.in, 0xF8);
  const auto out = d.dates(d.out, 0xF8);
  REQUIRE(out.size() == 18);
  for(std::size_t i = 0; i < out.size(); i++)
    CHECK(out[i] == in[i + 6]);
}

TEST_CASE("MIDI Sync In preset: drop song position", "[presets][midi_sync_in]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Drop song position");
  d.start_and_clock(12);
  d.send({0xF2, 8, 0});
  d.tick();
  CHECK(d.count(0xF2) == 0);
  CHECK(d.count(0xF8) == 12);
  CHECK(d.count(0xFA) == 1);
  // The position is still decoded.
  CHECK(d.fx.outputs.song_position.value == 8);
}
