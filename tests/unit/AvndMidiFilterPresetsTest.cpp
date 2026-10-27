// The Midi filter presets of the default package load onto the object and do
// what their names say.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiFilter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <vector>

using Filter = mtk::MidiFilter;

namespace
{
using bytes = std::vector<int>;
auto kinds_of(const char* text)
{
  return Filter::ins::kinds_grid::from_text(text);
}
using list = std::vector<bytes>;

libremidi::message msg(std::initializer_list<unsigned char> b, int ts = 0)
{
  libremidi::message m{b};
  m.timestamp = ts;
  return m;
}
bytes on(int ch, int n, int v) { return {0x90 + ch - 1, n, v}; }
bytes off(int ch, int n) { return {0x80 + ch - 1, n, 0}; }
bytes cc(int ch, int n, int v) { return {0xB0 + ch - 1, n, v}; }

const std::vector<midi_presets::preset>& all()
{
  static const auto p
      = midi_presets::load("Midi filter", "f66d6cf7-693a-47d4-a249-5978cc946e43");
  return p;
}

struct driver
{
  Filter fx;
  list main, rejected;
  //! Sounding notes per (status channel, note), one map per output: a note
  //! started on one output has to end on that same output.
  std::map<std::pair<int, int>, int> sounding_main, sounding_rejected;

  explicit driver(const char* name)
  {
    fx.outputs.raw.call.function = [](void*, int64_t, auto) {};
    fx.outputs.normalized.call.function = [](void*, int64_t, auto) {};
    fx.outputs.poly_raw.call.function = [](void*, int64_t, auto) {};

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

  static void track(std::map<std::pair<int, int>, int>& sounding, const libremidi::message& m)
  {
    const int st = m.bytes[0] & 0xF0;
    const auto key = std::make_pair(m.bytes[0] & 0x0F, int(m.bytes[1]));
    if(st == 0x90 && m.bytes[2] > 0)
      sounding[key]++;
    else if(st == 0x80 || st == 0x90)
      sounding[key]--;
  }

  //! Feeds the messages at increasing timestamps in one tick.
  void run(const list& in, int frames = 64)
  {
    main.clear();
    rejected.clear();
    int ts = 0;
    for(auto& b : in)
    {
      libremidi::message m;
      for(int x : b)
        m.bytes.push_back(uint8_t(x));
      m.timestamp = ts++;
      fx.inputs.midi.midi_messages.push_back(m);
    }
    fx();
    for(auto& m : fx.outputs.midi)
    {
      CHECK(m.timestamp >= 0);
      CHECK(m.timestamp < frames);
      main.push_back(bytes(m.bytes.begin(), m.bytes.end()));
      if(m.size() == 3)
        track(sounding_main, m);
    }
    for(auto& m : fx.outputs.rejected)
    {
      CHECK(m.timestamp >= 0);
      CHECK(m.timestamp < frames);
      rejected.push_back(bytes(m.bytes.begin(), m.bytes.end()));
      if(m.size() == 3)
        track(sounding_rejected, m);
    }
    // Nothing is lost: every message goes to one of the two outputs
    CHECK(main.size() + rejected.size() == in.size());
    fx.outputs.midi.midi_messages.clear();
    fx.outputs.rejected.midi_messages.clear();
    fx.inputs.midi.midi_messages.clear();
  }

  void check_paired()
  {
    for(auto [k, n] : sounding_main)
    {
      INFO("main: channel " << k.first + 1 << " note " << k.second);
      CHECK(n == 0);
    }
    for(auto [k, n] : sounding_rejected)
    {
      INFO("rejected: channel " << k.first + 1 << " note " << k.second);
      CHECK(n == 0);
    }
  }
};
}

TEST_CASE("Midi filter presets load", "[presets][midi_filter]")
{
  REQUIRE(all().size() >= 6);
  for(auto& p : all())
  {
    INFO(p.name.toStdString());
    Filter fx;
    CHECK(midi_presets::apply(fx.inputs, p).isEmpty());
    CHECK(!p.category.isEmpty());
  }
}

TEST_CASE("Midi filter preset: keyboard split at C4", "[presets][midi_filter]")
{
  {
    driver d{"Keyboard split at C4, lower half"};
    d.run({on(1, 59, 100), on(1, 60, 100), cc(1, 64, 127), off(1, 59), off(1, 60)});
    CHECK(d.main == list{on(1, 59, 100), cc(1, 64, 127), off(1, 59)});
    CHECK(d.rejected == list{on(1, 60, 100), off(1, 60)});
    d.check_paired();
  }
  {
    driver d{"Keyboard split at C4, upper half"};
    d.run({on(1, 59, 100), on(1, 60, 100), cc(1, 64, 127), off(1, 59), off(1, 60)});
    CHECK(d.main == list{on(1, 60, 100), cc(1, 64, 127), off(1, 60)});
    CHECK(d.rejected == list{on(1, 59, 100), off(1, 59)});
    d.check_paired();
  }
}

TEST_CASE("Midi filter preset: bass below C3 on channel 2", "[presets][midi_filter]")
{
  driver d{"Bass below C3 on channel 2"};
  d.run({on(1, 47, 90), on(1, 48, 90), on(2, 40, 90)});
  CHECK(d.main == list{on(2, 47, 90)});
  CHECK(d.rejected == list{on(1, 48, 90), on(2, 40, 90)});
  d.run({off(1, 47), off(1, 48), off(2, 40)});
  CHECK(d.main == list{off(2, 47)});
  CHECK(d.rejected == list{off(1, 48), off(2, 40)});
  d.check_paired();
}

TEST_CASE("Midi filter preset: channel selections", "[presets][midi_filter]")
{
  {
    driver d{"Channel 1 only"};
    d.run({on(1, 60, 100), on(2, 60, 100), {0xF8}, off(1, 60), off(2, 60)});
    CHECK(d.main == list{on(1, 60, 100), {0xF8}, off(1, 60)});
    CHECK(d.rejected == list{on(2, 60, 100), off(2, 60)});
    d.check_paired();
  }
  {
    driver d{"Drums on channel 10 only"};
    d.run({on(10, 36, 100), on(1, 60, 100), cc(10, 7, 90), off(10, 36), off(1, 60)});
    CHECK(d.main == list{on(10, 36, 100), cc(10, 7, 90), off(10, 36)});
    CHECK(d.rejected == list{on(1, 60, 100), off(1, 60)});
    d.check_paired();
  }
  {
    driver d{"Drop channel 10"};
    d.run({on(10, 36, 100), on(1, 60, 100), off(10, 36), off(1, 60)});
    CHECK(d.main == list{on(1, 60, 100), off(1, 60)});
    CHECK(d.rejected == list{on(10, 36, 100), off(10, 36)});
    d.check_paired();
  }
}

TEST_CASE("Midi filter preset: clean up", "[presets][midi_filter]")
{
  {
    driver d{"Drop sysex and clock"};
    d.run({{0xF0, 0x7E, 0x7F, 0xF7}, {0xF8}, {0xFA}, on(1, 60, 100), {0xF2, 0, 0},
           off(1, 60)});
    CHECK(d.main == list{{0xFA}, on(1, 60, 100), {0xF2, 0, 0}, off(1, 60)});
    CHECK(d.rejected == list{{0xF0, 0x7E, 0x7F, 0xF7}, {0xF8}});
  }
  {
    driver d{"Notes only"};
    d.run({on(1, 60, 100), cc(1, 1, 20), {0xE0, 0, 0x40}, {0xF8}, off(1, 60)});
    CHECK(d.main == list{on(1, 60, 100), off(1, 60)});
    CHECK(d.rejected == list{cc(1, 1, 20), {0xE0, 0, 0x40}, {0xF8}});
  }
  {
    driver d{"Drop aftertouch"};
    d.run({on(1, 60, 100), {0xD0, 50}, {0xA0, 60, 50}, cc(1, 1, 20), off(1, 60)});
    CHECK(d.main == list{on(1, 60, 100), cc(1, 1, 20), off(1, 60)});
    CHECK(d.rejected == list{{0xD0, 50}, {0xA0, 60, 50}});
  }
  {
    driver d{"Drop active sensing"};
    d.run({{0xFE}, {0xF8}, on(1, 60, 100), off(1, 60)});
    CHECK(d.main == list{{0xF8}, on(1, 60, 100), off(1, 60)});
    CHECK(d.rejected == list{{0xFE}});
  }
}

TEST_CASE("Midi filter preset: remaps", "[presets][midi_filter]")
{
  {
    driver d{"Channel 1 to 10"};
    d.run({on(1, 60, 100), on(2, 62, 100)});
    CHECK(d.main == list{on(10, 60, 100), on(2, 62, 100)});
    d.fx.inputs.channel_map.value = {}; // the note-off follows its note-on
    d.run({off(1, 60), off(2, 62)});
    CHECK(d.main == list{off(10, 60), off(2, 62)});
    d.check_paired();
  }
  {
    driver d{"Octave up"};
    d.run({on(1, 60, 100), on(1, 0, 100), on(1, 120, 100)});
    CHECK(d.main == list{on(1, 72, 100), on(1, 12, 100)});
    CHECK(d.rejected == list{on(1, 120, 100)});
    d.run({off(1, 60), off(1, 0), off(1, 120)});
    CHECK(d.main == list{off(1, 72), off(1, 12)});
    CHECK(d.rejected == list{off(1, 120)});
    d.check_paired();
  }
  {
    driver d{"Octave down"};
    d.run({on(1, 60, 100), on(1, 5, 100), off(1, 60), off(1, 5)});
    CHECK(d.main == list{on(1, 48, 100), off(1, 48)});
    CHECK(d.rejected == list{on(1, 5, 100), off(1, 5)});
    d.check_paired();
  }
  {
    driver d{"Mod wheel to expression"};
    d.run({cc(1, 1, 50), cc(1, 7, 100), on(1, 60, 100), off(1, 60)});
    CHECK(d.main == list{cc(1, 11, 50), cc(1, 7, 100), on(1, 60, 100), off(1, 60)});
    CHECK(d.rejected.empty());
  }
}

TEST_CASE("Midi filter preset: ranges", "[presets][midi_filter]")
{
  {
    driver d{"Soft notes only"};
    d.run({on(1, 60, 50), on(1, 62, 100), off(1, 60), off(1, 62)});
    CHECK(d.main == list{on(1, 60, 50), off(1, 60)});
    CHECK(d.rejected == list{on(1, 62, 100), off(1, 62)});
    d.check_paired();
  }
  {
    driver d{"Pedals CC 64-69 only"};
    d.run({cc(1, 64, 127), cc(1, 67, 0), cc(1, 1, 20), on(1, 60, 100), off(1, 60)});
    CHECK(d.main == list{cc(1, 64, 127), cc(1, 67, 0)});
    CHECK(d.rejected == list{cc(1, 1, 20), on(1, 60, 100), off(1, 60)});
  }
}

TEST_CASE("Midi filter presets keep notes paired", "[presets][midi_filter]")
{
  for(auto& p : all())
  {
    const auto name = p.name.toStdString();
    INFO(name);
    driver d{name.c_str()};
    for(int round = 0; round < 4; round++)
    {
      list in;
      for(int n = 0; n < 128; n += 7)
        in.push_back(on(1 + (n / 7) % 16, n, 1 + (n * 5) % 127));
      d.run(in);
      in.clear();
      for(int n = 0; n < 128; n += 7)
        in.push_back(off(1 + (n / 7) % 16, n));
      d.run(in);
    }
    d.check_paired();
  }
}

// Variant of "Notes only" with the documented token "noteon": the note-offs
// should follow their note-ons to the main output.
TEST_CASE("Midi filter: Types noteon keeps note-offs paired", "[presets][midi_filter]")
{
  driver d{"Notes only"};
  d.fx.inputs.kinds.value = kinds_of("noteon");
  d.run({on(1, 60, 100), off(1, 60)});
  CHECK(d.main == list{on(1, 60, 100), off(1, 60)});
  CHECK(d.rejected.empty());
}
