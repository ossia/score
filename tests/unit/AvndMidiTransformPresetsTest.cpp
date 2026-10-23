// The Midi transform presets of the default package load onto the object and
// do what their names say.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiTransform.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <vector>

using T = mtk::MidiTransform;

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
      = midi_presets::load("Midi transform", "1680d787-cb5d-4df3-8783-8bca4c3cad27");
  return p;
}

struct driver
{
  T fx;
  list out;
  std::map<std::pair<int, int>, int> sounding;

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

  void run(const list& in, bool stop = false, int frames = 64)
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
    if(stop)
      fx.stop();
    fx(halp::tick_musical{.frames = frames});
    for(auto& m : fx.outputs.midi)
    {
      CHECK(m.timestamp >= 0);
      CHECK(m.timestamp < frames);
      out.push_back(bytes(m.bytes.begin(), m.bytes.end()));
      const int st = m.bytes[0] & 0xF0;
      if(m.size() == 3 && (st == 0x90 || st == 0x80))
      {
        const auto key = std::make_pair(m.bytes[0] & 0x0F, int(m.bytes[1]));
        if(st == 0x90 && m.bytes[2] > 0)
          sounding[key]++;
        else
          sounding[key]--;
      }
    }
    fx.outputs.midi.midi_messages.clear();
    fx.inputs.midi.midi_messages.clear();
  }

  void check_paired()
  {
    for(auto [k, n] : sounding)
    {
      INFO("channel " << k.first + 1 << " note " << k.second);
      CHECK(n == 0);
    }
  }
};
}

TEST_CASE("Midi transform presets load", "[presets][midi_transform]")
{
  REQUIRE(all().size() >= 6);
  for(auto& p : all())
  {
    INFO(p.name.toStdString());
    T fx;
    CHECK(midi_presets::apply(fx.inputs, p).isEmpty());
    CHECK(!p.category.isEmpty());
  }
}

TEST_CASE("Midi transform preset: notes to controls", "[presets][midi_transform]")
{
  {
    driver d{"Notes to CC with velocity"};
    d.run({on(1, 60, 100), on(2, 62, 30)});
    CHECK(d.out == list{cc(1, 60, 100), cc(2, 62, 30)});
    d.run({off(1, 60), off(2, 62)});
    CHECK(d.out.empty());
  }
  {
    driver d{"Velocity to mod wheel"};
    d.run({on(1, 60, 90)});
    CHECK(d.out == list{on(1, 60, 90), cc(1, 1, 90)});
    d.run({off(1, 60)});
    CHECK(d.out == list{off(1, 60)});
    d.check_paired();
  }
  {
    driver d{"Channel 10 notes to CC"};
    d.run({on(10, 36, 100), on(1, 60, 80)});
    CHECK(d.out == list{cc(10, 36, 100), on(1, 60, 80)});
    d.run({off(10, 36), off(1, 60)});
    CHECK(d.out == list{off(1, 60)});
    d.check_paired();
  }
}

TEST_CASE("Midi transform preset: program change plays a note", "[presets][midi_transform]")
{
  driver d{"Program change plays a note"};
  d.run({{0xC0, 5}});
  CHECK(d.out == list{on(1, 5, 100)});
  d.run({{0xC0, 7}});
  CHECK(d.out == list{off(1, 5), on(1, 7, 100)});
  d.run({}, true);
  CHECK(d.out == list{off(1, 7)});
  d.check_paired();
}

TEST_CASE("Midi transform preset: controls to notes", "[presets][midi_transform]")
{
  {
    driver d{"CC 36-51 pads to drum notes"};
    d.run({cc(1, 36, 100), cc(1, 20, 50), cc(1, 51, 64)});
    CHECK(d.out == list{on(10, 36, 100), cc(1, 20, 50), on(10, 51, 64)});
    d.run({cc(1, 36, 0), cc(1, 51, 0)});
    CHECK(d.out.size() == 2);
    if(d.out.size() == 2)
    {
      CHECK((d.out[0][0] & 0xF0) == 0x80);
      CHECK(d.out[0][0] == 0x89);
      CHECK(d.out[0][1] == 36);
      CHECK(d.out[1][0] == 0x89);
      CHECK(d.out[1][1] == 51);
    }
    d.check_paired();
  }
  {
    driver d{"Sustain pedal plays C4"};
    d.run({cc(1, 64, 127), cc(1, 1, 30)});
    CHECK(d.out == list{on(1, 60, 127), cc(1, 1, 30)});
    d.run({cc(1, 64, 0)});
    CHECK(d.out.size() == 1);
    if(d.out.size() == 1)
    {
      CHECK(d.out[0][0] == 0x80);
      CHECK(d.out[0][1] == 60);
    }
    d.check_paired();
  }
}

TEST_CASE("Midi transform preset: controllers", "[presets][midi_transform]")
{
  {
    driver d{"Aftertouch to mod wheel"};
    d.run({{0xD0, 50}, {0xD3, 127}});
    CHECK(d.out == list{cc(1, 1, 50), cc(4, 1, 127)});
  }
  {
    driver d{"Pitch bend to CC 2"};
    d.run({{0xE0, 0, 0x40}, {0xE0, 0x7F, 0x7F}, {0xE0, 0, 0}});
    CHECK(d.out == list{cc(1, 2, 64), cc(1, 2, 127), cc(1, 2, 0)});
  }
  {
    driver d{"CCs to channel 2"};
    d.run({cc(1, 7, 100), cc(5, 10, 3), on(1, 60, 100), off(1, 60)});
    CHECK(d.out == list{cc(2, 7, 100), cc(2, 10, 3), on(1, 60, 100), off(1, 60)});
  }
  {
    driver d{"Mod wheel to filter cutoff"};
    d.run({cc(1, 1, 80), cc(1, 7, 100)});
    CHECK(d.out == list{cc(1, 74, 80), cc(1, 7, 100)});
  }
}

TEST_CASE("Midi transform preset: system messages to notes", "[presets][midi_transform]")
{
  {
    driver d{"Clock to note on channel 10"};
    d.run({{0xF8}});
    CHECK(d.out == list{{0xF8}, on(10, 76, 100)});
    d.run({{0xF8}});
    CHECK(d.out == list{{0xF8}, off(10, 76), on(10, 76, 100)});
    d.run({}, true);
    CHECK(d.out == list{off(10, 76)});
    d.check_paired();
  }
  {
    driver d{"Start plays a note"};
    d.run({{0xFA}, {0xF8}});
    CHECK(d.out == list{{0xFA}, on(10, 49, 110), {0xF8}});
    d.run({}, true);
    CHECK(d.out == list{off(10, 49)});
    d.check_paired();
  }
}

TEST_CASE("Midi transform preset: filters", "[presets][midi_transform]")
{
  {
    driver d{"Drop notes below velocity 41"};
    d.run({on(1, 60, 30), on(1, 62, 100), on(1, 64, 41)});
    CHECK(d.out == list{on(1, 62, 100), on(1, 64, 41)});
    d.run({off(1, 60), off(1, 62), off(1, 64)});
    CHECK(d.out == list{off(1, 62), off(1, 64)});
    d.check_paired();
  }
  {
    driver d{"Drop notes outside C2-C5"};
    d.run({on(1, 30, 100), on(1, 36, 100), on(1, 72, 100), on(1, 73, 100)});
    CHECK(d.out == list{on(1, 36, 100), on(1, 72, 100)});
    d.run({off(1, 30), off(1, 36), off(1, 72), off(1, 73)});
    CHECK(d.out == list{off(1, 36), off(1, 72)});
    d.check_paired();
  }
}

TEST_CASE("Midi transform presets keep notes paired", "[presets][midi_transform]")
{
  for(auto& p : all())
  {
    const auto name = p.name.toStdString();
    INFO(name);
    driver d{name.c_str()};
    for(int round = 0; round < 3; round++)
    {
      list in;
      for(int n = 24; n < 100; n += 5)
        in.push_back(on(1 + n % 3, n, 1 + (n * 7) % 127));
      in.push_back({0xF8});
      in.push_back({0xFA});
      in.push_back({0xC0, 3});
      in.push_back(cc(1, 64, 127));
      in.push_back(cc(1, 40, 90));
      d.run(in);
      in.clear();
      for(int n = 24; n < 100; n += 5)
        in.push_back(off(1 + n % 3, n));
      in.push_back(cc(1, 64, 0));
      in.push_back(cc(1, 40, 0));
      d.run(in);
    }
    d.run({}, true);
    d.check_paired();
  }
}
