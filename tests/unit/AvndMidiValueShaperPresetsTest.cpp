// The Midi value shaper presets of the default package load onto the object
// and shape values the way their names say.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiValueShaper.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <vector>

using VS = mtk::MidiValueShaper;

namespace
{
using bytes = std::vector<int>;
using list = std::vector<bytes>;

bytes on(int ch, int n, int v) { return {0x90 + ch - 1, n, v}; }
bytes off(int ch, int n, int v = 0) { return {0x80 + ch - 1, n, v}; }
bytes cc(int ch, int n, int v) { return {0xB0 + ch - 1, n, v}; }

const std::vector<midi_presets::preset>& all()
{
  static const auto p
      = midi_presets::load("Midi value shaper", "6ed46ee7-8d04-4e46-8a68-e2c6633fd217");
  return p;
}

struct driver
{
  VS fx;
  list out;
  int64_t pos{};

  explicit driver(const char* name)
  {
    const midi_presets::preset* found{};
    for(auto& p : all())
      if(p.name == name)
        found = &p;
    INFO(name);
    REQUIRE(found);
    const auto errors = midi_presets::apply(fx.inputs, *found);
    INFO(errors.join("; ").toStdString());
    REQUIRE(errors.isEmpty());
  }

  list run(const list& in, int frames = 64)
  {
    out.clear();
    int ts = 0;
    for(auto& b : in)
    {
      libremidi::message m;
      for(int x : b)
        m.bytes.push_back(uint8_t(x));
      m.timestamp = ts++;
      fx.inputs.midi.midi_messages.push_back(m);
    }
    fx(halp::tick_musical{.frames = frames, .position_in_frames = pos});
    pos += frames;
    for(auto& m : fx.outputs.midi)
    {
      CHECK(m.timestamp >= 0);
      CHECK(m.timestamp < frames);
      out.push_back(bytes(m.bytes.begin(), m.bytes.end()));
    }
    CHECK(out.size() == in.size());
    fx.outputs.midi.midi_messages.clear();
    fx.inputs.midi.midi_messages.clear();
    return out;
  }

  //! Velocity a note-on of `v` on note `n` comes out with.
  int vel(int v, int n = 60) { return run({on(1, n, v)}).at(0).at(2); }
  //! Value of a controller.
  int ctl(int num, int v) { return run({cc(1, num, v)}).at(0).at(2); }
};
}

TEST_CASE("Midi value shaper presets load and keep notes", "[presets][midi_value_shaper]")
{
  REQUIRE(all().size() >= 6);
  for(auto& p : all())
  {
    const auto name = p.name.toStdString();
    INFO(name);
    CHECK(!p.category.isEmpty());
    driver d{name.c_str()};
    for(int v = 1; v < 128; v += 3)
    {
      const auto o = d.run({on(1, v, v), off(1, v)});
      // A note-on stays a note-on of the same note, its note-off a note-off
      CHECK(o[0][0] == 0x90);
      CHECK(o[0][1] == v);
      CHECK(o[0][2] >= 1);
      CHECK(o[1][0] == 0x80);
      CHECK(o[1][1] == v);
    }
  }
}

TEST_CASE("Midi value shaper preset: velocity compressor", "[presets][midi_value_shaper]")
{
  driver d{"Velocity compressor"};
  const int lo = d.vel(32), mid = d.vel(64), hi = d.vel(127);
  CHECK(lo > 32);
  CHECK(hi < 127);
  CHECK(hi > mid);
  CHECK(mid > lo);
  CHECK(hi - mid < (127 - 64) / 2);
}

TEST_CASE("Midi value shaper preset: velocity curves", "[presets][midi_value_shaper]")
{
  {
    driver d{"Soft velocity curve"};
    CHECK(d.vel(32) > 45);
    CHECK(d.vel(127) == 127);
    int prev = 0;
    for(int v = 1; v < 128; v++)
    {
      const int o = d.vel(v);
      CHECK(o >= v);
      CHECK(o >= prev);
      prev = o;
    }
  }
  {
    driver d{"Hard velocity curve"};
    CHECK(d.vel(64) < 50);
    CHECK(d.vel(127) == 127);
    int prev = 0;
    for(int v = 1; v < 128; v++)
    {
      const int o = d.vel(v);
      CHECK(o <= v);
      CHECK(o >= prev);
      prev = o;
    }
  }
  {
    driver d{"Aftertouch soft curve"};
    const auto o = d.run({{0xD0, 32}}).at(0);
    CHECK(o[0] == 0xD0);
    CHECK(o[1] > 45);
    CHECK(d.run({{0xD0, 127}}).at(0) == bytes{0xD0, 127});
    CHECK(d.run({{0xD0, 0}}).at(0) == bytes{0xD0, 0});
    // Velocities untouched
    CHECK(d.vel(32) == 32);
  }
}

TEST_CASE("Midi value shaper preset: fixed and limited velocities", "[presets][midi_value_shaper]")
{
  {
    driver d{"Fixed velocity 100"};
    for(int v : {1, 50, 100, 127})
      CHECK(d.vel(v) == 100);
    CHECK(d.run({off(1, 60, 33)}).at(0) == off(1, 60, 33));
  }
  {
    driver d{"Velocity limiter 100"};
    CHECK(d.vel(127) == 100);
    CHECK(d.vel(101) == 100);
    CHECK(d.vel(100) == 100);
    CHECK(d.vel(50) == 50);
    CHECK(d.vel(1) == 1);
  }
  {
    driver d{"Velocity floor 40"};
    for(int v = 1; v < 128; v++)
      CHECK(d.vel(v) >= 40);
    CHECK(d.vel(127) == 127);
  }
  {
    driver d{"Release velocity fixed 64"};
    CHECK(d.run({off(1, 60, 10)}).at(0) == off(1, 60, 64));
    CHECK(d.run({off(1, 60, 127)}).at(0) == off(1, 60, 64));
    CHECK(d.vel(20) == 20);
    // A note-on with velocity 0 stays a note-off
    CHECK(d.run({on(1, 60, 0)}).at(0) == on(1, 60, 0));
  }
}

TEST_CASE("Midi value shaper preset: louder bass", "[presets][midi_value_shaper]")
{
  driver d{"Louder bass softer treble"};
  CHECK(d.vel(80, 24) == 104);
  CHECK(d.vel(80, 60) == 80);
  CHECK(d.vel(80, 96) == 64);
  CHECK(d.vel(80, 36) > 80);
  CHECK(d.vel(80, 84) < 80);
}

TEST_CASE("Midi value shaper preset: random velocity", "[presets][midi_value_shaper]")
{
  driver d{"Random velocity spread 10"};
  std::map<int, int> seen;
  double sum = 0;
  for(int i = 0; i < 400; i++)
  {
    const int o = d.vel(64);
    CHECK(o >= 54);
    CHECK(o <= 74);
    seen[o]++;
    sum += o;
  }
  CHECK(seen.size() > 10);
  CHECK(sum / 400 > 61);
  CHECK(sum / 400 < 67);
}

TEST_CASE("Midi value shaper preset: controllers", "[presets][midi_value_shaper]")
{
  {
    driver d{"Mod wheel to half range"};
    CHECK(d.ctl(1, 127) == 64);
    CHECK(d.ctl(1, 0) == 0);
    CHECK(d.ctl(1, 64) == 32);
    CHECK(d.ctl(7, 127) == 127);
    CHECK(d.vel(100) == 100);
  }
  {
    driver d{"Expression inverted"};
    CHECK(d.ctl(11, 0) == 127);
    CHECK(d.ctl(11, 127) == 0);
    CHECK(d.ctl(11, 100) == 27);
    CHECK(d.ctl(1, 100) == 100);
  }
}

TEST_CASE("Midi value shaper preset: pitch bend range halved", "[presets][midi_value_shaper]")
{
  driver d{"Pitch bend range halved"};
  auto bend = [&](int v) {
    const auto o = d.run({{0xE0, v & 0x7F, (v >> 7) & 0x7F}}).at(0);
    return o[1] | (o[2] << 7);
  };
  CHECK(bend(8192) == 8192);
  CHECK(std::abs(bend(16383) - 12288) <= 1);
  CHECK(std::abs(bend(0) - 4096) <= 1);
  CHECK(std::abs(bend(12288) - 10240) <= 2);
  CHECK(d.vel(100) == 100);
}
