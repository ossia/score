#include <Advanced/MidiScaler/MidiValueShaper.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using Shaper = mtk::MidiValueShaper;

namespace
{
struct driver
{
  Shaper fx;
  int64_t position{};

  std::vector<libremidi::message> run(std::vector<libremidi::message> in, int frames = 512)
  {
    fx.inputs.midi.midi_messages.clear();
    fx.outputs.midi.midi_messages.clear();
    for(auto& m : in)
      fx.inputs.midi.push_back(m);
    fx(halp::tick_musical{.frames = frames, .position_in_frames = position});
    position += frames;
    std::vector<libremidi::message> out;
    for(auto& m : fx.outputs.midi.midi_messages)
    {
      if(frames > 0)
      {
        REQUIRE(m.timestamp >= 0);
        REQUIRE(m.timestamp < frames);
      }
      else
      {
        REQUIRE(m.timestamp == 0);
      }
      out.push_back(m);
    }
    return out;
  }

  int velocity(int vel, int note = 60)
  {
    auto out = run({libremidi::channel_events::note_on(1, note, vel)});
    REQUIRE(out.size() == 1);
    return out[0].bytes[2];
  }

  int cc(int number, int value, int channel = 1)
  {
    auto out = run({libremidi::channel_events::control_change(channel, number, value)});
    REQUIRE(out.size() == 1);
    return out[0].bytes[2];
  }

  int bend(int value)
  {
    auto out = run({libremidi::channel_events::pitch_bend(1, value)});
    REQUIRE(out.size() == 1);
    return (out[0].bytes[2] << 7) | out[0].bytes[1];
  }
};

libremidi::message at(libremidi::message m, int ts)
{
  m.timestamp = ts;
  return m;
}
}

TEST_CASE("mtk::MidiValueShaper is the identity at its defaults", "[avnd][midi][shaper]")
{
  for(auto kind :
      {Shaper::Velocity, Shaper::Release, Shaper::CC, Shaper::PitchBend,
       Shaper::Pressure, Shaper::PolyPressure})
  {
    driver d;
    d.fx.inputs.kind.value = kind;

    std::vector<libremidi::message> in;
    for(int v = 0; v < 128; v++)
    {
      in.push_back(at(libremidi::channel_events::note_on(1, v, v), v));
      in.push_back(at(libremidi::channel_events::note_off(2, v, v), v));
      in.push_back(at(libremidi::channel_events::control_change(3, v, v), v));
      in.push_back(at(libremidi::channel_events::aftertouch(4, v), v));
      in.push_back(at(libremidi::channel_events::poly_pressure(5, v, v), v));
    }
    const auto out = d.run(in);
    REQUIRE(out.size() == in.size());
    for(std::size_t i = 0; i < in.size(); i++)
    {
      REQUIRE(out[i].bytes == in[i].bytes);
      REQUIRE(out[i].timestamp == in[i].timestamp);
    }
  }
}

TEST_CASE("mtk::MidiValueShaper keeps the 14 bits of pitch bend", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.kind.value = Shaper::PitchBend;

  for(int v = 0; v < 16384; v++)
    REQUIRE(d.bend(v) == v);

  SECTION("gain scales around the center, at full resolution")
  {
    d.fx.inputs.gain.value = 0.5f;
    REQUIRE(d.bend(8192) == 8192);
    REQUIRE(d.bend(16383) == 8192 + 4096);
    REQUIRE(d.bend(0) == 4096);
    REQUIRE(d.bend(8200) == 8196);
    REQUIRE(d.bend(8184) == 8188);
  }

  SECTION("fixed 64 is the center")
  {
    d.fx.inputs.fixed.value = true;
    d.fx.inputs.fixed_value.value = 64.f;
    REQUIRE(d.bend(123) == 8192);
  }

  SECTION("curve keeps the ends and the center")
  {
    d.fx.inputs.curve.value = 0.7f;
    REQUIRE(d.bend(0) == 0);
    REQUIRE(d.bend(8192) == 8192);
    REQUIRE(d.bend(16383) == 16383);
    // Both halves are shaped alike
    const int up = d.bend(8192 + 2048) - 8192;
    const int down = 8192 - d.bend(8192 - 2048);
    REQUIRE(std::abs(up - down) <= 2);
    REQUIRE(up > 2048);
  }
}

TEST_CASE("mtk::MidiValueShaper amplifies and offsets velocities", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.gain.value = 1.5f;
  REQUIRE(d.velocity(40) == 60);
  REQUIRE(d.velocity(100) == 127);

  d.fx.inputs.gain.value = 1.f;
  d.fx.inputs.offset.value = -20.f;
  REQUIRE(d.velocity(50) == 30);
  // A note-on never becomes a note-off
  REQUIRE(d.velocity(10) == 1);

  d.fx.inputs.offset.value = 0.f;
  d.fx.inputs.gain.value = 0.f;
  REQUIRE(d.velocity(100) == 1);
}

TEST_CASE("mtk::MidiValueShaper rescales with the input clipped first", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.in_range.value = {20.f, 100.f};
  d.fx.inputs.out_range.value = {40.f, 80.f};
  REQUIRE(d.velocity(20) == 40);
  REQUIRE(d.velocity(60) == 60);
  REQUIRE(d.velocity(100) == 80);
  REQUIRE(d.velocity(5) == 40);
  REQUIRE(d.velocity(127) == 80);

  SECTION("inverted output range")
  {
    d.fx.inputs.out_range.value = {80.f, 40.f};
    REQUIRE(d.velocity(20) == 80);
    REQUIRE(d.velocity(100) == 40);
  }
}

TEST_CASE("mtk::MidiValueShaper curve is monotonic and keeps the ends", "[avnd][midi][shaper]")
{
  for(float c : {-1.f, -0.4f, 0.3f, 1.f})
  {
    driver d;
    d.fx.inputs.kind.value = Shaper::CC;
    d.fx.inputs.curve.value = c;
    int prev = -1;
    for(int v = 0; v < 128; v++)
    {
      const int o = d.cc(7, v);
      REQUIRE(o >= prev);
      prev = o;
      if(c > 0 && v > 0 && v < 127)
        REQUIRE(o >= v);
      if(c < 0 && v > 0 && v < 127)
        REQUIRE(o <= v);
    }
    REQUIRE(d.cc(7, 0) == 0);
    REQUIRE(d.cc(7, 127) == 127);
  }
}

TEST_CASE("mtk::MidiValueShaper compresses and expands above the threshold", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.threshold.value = 64.f;
  d.fx.inputs.ratio.value = 4.f;
  REQUIRE(d.velocity(30) == 30);
  REQUIRE(d.velocity(64) == 64);
  REQUIRE(d.velocity(124) == 79);

  d.fx.inputs.ratio.value = 0.5f;
  REQUIRE(d.velocity(30) == 30);
  REQUIRE(d.velocity(80) == 96);
  REQUIRE(d.velocity(120) == 127);
}

TEST_CASE("mtk::MidiValueShaper clips into a range", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.clip.value = {30.f, 90.f};
  REQUIRE(d.velocity(10) == 30);
  REQUIRE(d.velocity(50) == 50);
  REQUIRE(d.velocity(120) == 90);

  SECTION("a clip down to 0 still leaves note-ons at 1")
  {
    d.fx.inputs.clip.value = {0.f, 0.f};
    REQUIRE(d.velocity(100) == 1);
  }
}

TEST_CASE("mtk::MidiValueShaper key-tracked gain", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.low_gain.value = 0.5f;
  d.fx.inputs.mid_gain.value = 1.f;
  d.fx.inputs.high_gain.value = 1.5f;
  REQUIRE(d.velocity(80, 0) == 40);
  REQUIRE(d.velocity(80, 24) == 40);
  REQUIRE(d.velocity(80, 42) == 60);
  REQUIRE(d.velocity(80, 60) == 80);
  REQUIRE(d.velocity(80, 78) == 100);
  REQUIRE(d.velocity(80, 96) == 120);
  REQUIRE(d.velocity(80, 127) == 120);

  // Values without a note are not key-tracked
  d.fx.inputs.kind.value = Shaper::CC;
  REQUIRE(d.cc(1, 80) == 80);

  d.fx.inputs.kind.value = Shaper::PolyPressure;
  auto out = d.run({libremidi::channel_events::poly_pressure(1, 24, 80)});
  REQUIRE(out[0].bytes[2] == 40);
}

TEST_CASE("mtk::MidiValueShaper fixed value", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.fixed.value = true;
  d.fx.inputs.fixed_value.value = 90.f;
  REQUIRE(d.velocity(1) == 90);
  REQUIRE(d.velocity(127) == 90);

  // The chain goes on after it
  d.fx.inputs.gain.value = 0.5f;
  REQUIRE(d.velocity(10) == 45);
}

TEST_CASE("mtk::MidiValueShaper random add is seeded and bounded", "[avnd][midi][shaper]")
{
  auto take = [](int seed) {
    driver d;
    d.fx.inputs.random.value = {-10.f, 10.f};
    d.fx.inputs.seed.value = seed;
    std::vector<int> r;
    for(int i = 0; i < 200; i++)
      r.push_back(d.velocity(64));
    return r;
  };

  const auto a = take(42);
  const auto b = take(42);
  const auto c = take(43);
  REQUIRE(a == b);
  REQUIRE(a != c);

  int lo = 127, hi = 0;
  for(int v : a)
  {
    REQUIRE(v >= 54);
    REQUIRE(v <= 74);
    lo = std::min(lo, v);
    hi = std::max(hi, v);
  }
  REQUIRE(lo < 58);
  REQUIRE(hi > 70);

  SECTION("a rewind or a stop restarts the take")
  {
    driver d;
    d.fx.inputs.random.value = {-10.f, 10.f};
    d.fx.inputs.seed.value = 42;
    std::vector<int> first;
    for(int i = 0; i < 20; i++)
      first.push_back(d.velocity(64));

    d.position = 0;
    std::vector<int> second;
    for(int i = 0; i < 20; i++)
      second.push_back(d.velocity(64));
    REQUIRE(first == second);

    d.fx.stop();
    REQUIRE(d.run({}, 0).empty());
    std::vector<int> third;
    for(int i = 0; i < 20; i++)
      third.push_back(d.velocity(64));
    REQUIRE(first == third);
  }

  SECTION("seed 0 keeps running across a stop")
  {
    driver d;
    d.fx.inputs.random.value = {-10.f, 10.f};
    std::vector<int> first;
    for(int i = 0; i < 20; i++)
      first.push_back(d.velocity(64));
    d.fx.stop();
    REQUIRE(d.run({}, 0).empty());
    std::vector<int> second;
    for(int i = 0; i < 20; i++)
      second.push_back(d.velocity(64));
    REQUIRE(first != second);
  }
}

TEST_CASE("mtk::MidiValueShaper only touches what it selects", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.kind.value = Shaper::CC;
  d.fx.inputs.number.value = 7;
  d.fx.inputs.channel.value = 2;
  d.fx.inputs.gain.value = 0.5f;

  REQUIRE(d.cc(7, 100, 2) == 50);
  REQUIRE(d.cc(7, 100, 1) == 100);
  REQUIRE(d.cc(8, 100, 2) == 100);

  // Everything else passes through unchanged and in order
  std::vector<libremidi::message> in{
      at(libremidi::channel_events::note_on(2, 60, 100), 0),
      at(libremidi::channel_events::pitch_bend(2, 1000), 1),
      at(libremidi::channel_events::program_change(2, 5), 2),
      at(libremidi::channel_events::control_change(2, 7, 100), 3),
      at(libremidi::message{0xF8}, 4),
      at(libremidi::channel_events::note_off(2, 60, 0), 5),
  };
  auto out = d.run(in);
  REQUIRE(out.size() == in.size());
  for(std::size_t i = 0; i < in.size(); i++)
  {
    REQUIRE(out[i].timestamp == in[i].timestamp);
    if(i == 3)
      REQUIRE(out[i].bytes[2] == 50);
    else
      REQUIRE(out[i].bytes == in[i].bytes);
  }

  SECTION("any controller, but never all-notes-off")
  {
    d.fx.inputs.number.value = -1;
    REQUIRE(d.cc(1, 100, 2) == 50);
    d.fx.inputs.fixed.value = true;
    REQUIRE(d.cc(123, 0, 2) == 0);
    REQUIRE(d.cc(120, 0, 2) == 0);
  }

  SECTION("any controller leaves bank select, RPN, NRPN and channel mode alone")
  {
    d.fx.inputs.number.value = -1;
    d.fx.inputs.gain.value = 1.f;
    d.fx.inputs.offset.value = 10.f;
    REQUIRE(d.cc(1, 50, 2) == 60);

    auto cc = [](int n, int v) { return libremidi::channel_events::control_change(2, n, v); };
    // Bank select, then pitch bend range (RPN 0) set to 2 semitones, an
    // increment, an NRPN select, and every channel mode message.
    std::vector<libremidi::message> in{
        cc(0, 1),   cc(32, 3),  cc(101, 0), cc(100, 0), cc(6, 2),   cc(38, 0),
        cc(96, 1),  cc(97, 1),  cc(99, 5),  cc(98, 7),  cc(101, 127), cc(100, 127),
        cc(121, 0), cc(122, 0), cc(124, 0), cc(125, 0), cc(126, 1), cc(127, 0)};
    auto out = d.run(in);
    REQUIRE(out.size() == in.size());
    for(std::size_t i = 0; i < in.size(); i++)
    {
      INFO("controller " << int(in[i].bytes[1]));
      CHECK(out[i].bytes == in[i].bytes);
    }

    // A selected number is reshaped whatever it is.
    d.fx.inputs.number.value = 6;
    REQUIRE(d.cc(6, 2, 2) == 12);
  }
}

TEST_CASE("mtk::MidiValueShaper release velocity", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.kind.value = Shaper::Release;
  d.fx.inputs.fixed.value = true;
  d.fx.inputs.fixed_value.value = 70.f;

  auto out = d.run(
      {libremidi::channel_events::note_on(1, 60, 100),
       libremidi::channel_events::note_off(1, 60, 10),
       libremidi::channel_events::note_on(1, 62, 0)});
  REQUIRE(out.size() == 3);
  REQUIRE(out[0].bytes[2] == 100);
  REQUIRE(out[1].bytes[2] == 70);
  // A velocity-0 note-on stays a note-off
  REQUIRE(out[2].bytes[0] == 0x90);
  REQUIRE(out[2].bytes[2] == 0);
}

TEST_CASE("mtk::MidiValueShaper pressure kinds", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.kind.value = Shaper::Pressure;
  d.fx.inputs.gain.value = 0.5f;
  auto out = d.run(
      {libremidi::channel_events::aftertouch(1, 100),
       libremidi::channel_events::poly_pressure(1, 60, 100)});
  REQUIRE(out[0].bytes[1] == 50);
  REQUIRE(out[1].bytes[2] == 100);

  d.fx.inputs.kind.value = Shaper::PolyPressure;
  d.fx.inputs.number.value = 60;
  out = d.run(
      {libremidi::channel_events::poly_pressure(1, 60, 100),
       libremidi::channel_events::poly_pressure(1, 61, 100),
       libremidi::channel_events::aftertouch(1, 100)});
  REQUIRE(out[0].bytes[2] == 50);
  REQUIRE(out[1].bytes[2] == 100);
  REQUIRE(out[2].bytes[1] == 100);
}

TEST_CASE("mtk::MidiValueShaper keeps note pairs whatever the controls do", "[avnd][midi][shaper]")
{
  driver d;
  d.fx.inputs.random.value = {-64.f, 64.f};
  d.fx.inputs.seed.value = 7;
  d.fx.inputs.offset.value = -60.f;
  int ons = 0, offs = 0;
  for(int i = 0; i < 500; i++)
  {
    d.fx.inputs.gain.value = float(i % 5) * 0.5f;
    auto out = d.run(
        {libremidi::channel_events::note_on(1, i % 128, 1 + i % 127),
         libremidi::channel_events::note_off(1, i % 128, 0),
         libremidi::channel_events::control_change(1, 123, 0)});
    REQUIRE(out.size() == 3);
    for(auto& m : out)
    {
      if(mtk::is_note_on(m))
        ons++;
      else if(mtk::is_note_off(m))
        offs++;
    }
    REQUIRE(mtk::is_all_notes_off(out[2]));
  }
  REQUIRE(ons == 500);
  REQUIRE(offs == 500);
}
