#include <Advanced/MidiScaler/MidiFilter.hpp>

#include "MidiChannelMask.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using Filter = mtk::MidiFilter;

namespace
{
using bytes = std::vector<int>;
//! The controls set as their old texts were.
auto kinds_of(const char* text)
{
  return Filter::ins::kinds_grid::from_text(text);
}
auto notes_of(const char* text)
{
  return Filter::ins::notes_list::from_text(text);
}
auto ccs_of(const char* text)
{
  return Filter::ins::ccs_list::from_text(text);
}

struct out_msg
{
  bytes b;
  int64_t ts{};
};

bytes to_bytes(const libremidi::message& m)
{
  return bytes(m.bytes.begin(), m.bytes.end());
}

libremidi::message on(int ch, int note, int vel, int ts = 0)
{
  auto m = libremidi::channel_events::note_on(ch, note, vel);
  m.timestamp = ts;
  return m;
}
libremidi::message off(int ch, int note, int ts = 0)
{
  auto m = libremidi::channel_events::note_off(ch, note, 0);
  m.timestamp = ts;
  return m;
}
libremidi::message cc(int ch, int num, int val, int ts = 0)
{
  auto m = libremidi::channel_events::control_change(ch, num, val);
  m.timestamp = ts;
  return m;
}
libremidi::message raw(std::initializer_list<unsigned char> b, int ts = 0)
{
  libremidi::message m{b};
  m.timestamp = ts;
  return m;
}

struct driver
{
  Filter fx;
  std::vector<out_msg> main, rejected;
  std::vector<int> raw_values;

  driver()
  {
    fx.outputs.raw.call.context = this;
    fx.outputs.raw.call.function
        = [](void* ctx, int64_t, ossia::variant<int, Filter::note_int> v) {
      auto& self = *static_cast<driver*>(ctx);
      if(auto p = ossia::get_if<int>(&v))
        self.raw_values.push_back(*p);
      else if(auto n = ossia::get_if<Filter::note_int>(&v))
        self.raw_values.push_back(n->note * 1000 + n->velocity);
    };
  }

  void in(const libremidi::message& m) { fx.inputs.midi.midi_messages.push_back(m); }

  void run(int frames = 64)
  {
    main.clear();
    rejected.clear();
    fx.outputs.midi.midi_messages.clear();
    fx.outputs.rejected.midi_messages.clear();
    fx();
    for(auto& m : fx.outputs.midi)
    {
      CHECK(m.timestamp >= 0);
      CHECK((frames == 0 ? m.timestamp == 0 : m.timestamp < frames));
      main.push_back({to_bytes(m), m.timestamp});
    }
    for(auto& m : fx.outputs.rejected)
    {
      CHECK(m.timestamp >= 0);
      rejected.push_back({to_bytes(m), m.timestamp});
    }
    fx.inputs.midi.midi_messages.clear();
  }

  static std::vector<bytes> b(const std::vector<out_msg>& v)
  {
    std::vector<bytes> r;
    for(auto& m : v)
      r.push_back(m.b);
    return r;
  }
};
}

TEST_CASE("Midi filter defaults keep only controllers, once each", "[midi_filter]")
{
  driver d;
  d.in(on(1, 60, 100, 1));
  d.in(cc(1, 7, 99, 2));
  d.in(raw({0xF8}, 3));
  d.in(raw({0xE0, 0, 64}, 4));
  d.in(off(1, 60, 5));
  d.run();

  REQUIRE(driver::b(d.main) == std::vector<bytes>{{0xB0, 7, 99}});
  CHECK(d.main[0].ts == 2);
  REQUIRE(
      driver::b(d.rejected)
      == std::vector<bytes>{{0x90, 60, 100}, {0xF8}, {0xE0, 0, 64}, {0x80, 60, 0}});
  CHECK(d.rejected[0].ts == 1);
  CHECK(d.rejected[3].ts == 5);
  // Index 0, mode Index: the raw output is the controller number
  CHECK(d.raw_values == std::vector<int>{7});
}

TEST_CASE("Midi filter legacy type, index and channel", "[midi_filter]")
{
  driver d;
  d.fx.inputs.filter.value = Filter::NoteAny;
  d.fx.inputs.index.value = 60;
  d.fx.inputs.channel.value = 2;
  d.in(on(2, 60, 100));
  d.in(on(2, 61, 100));
  d.in(on(1, 60, 100));
  d.in(cc(2, 60, 1));
  d.in(off(2, 60));
  d.run();
  CHECK(driver::b(d.main) == std::vector<bytes>{{0x91, 60, 100}, {0x81, 60, 0}});
  CHECK(
      driver::b(d.rejected)
      == std::vector<bytes>{{0x91, 61, 100}, {0x90, 60, 100}, {0xB1, 60, 1}});
  CHECK(d.raw_values == std::vector<int>{60, 60});

  SECTION("Note on type: the note-off of a kept note is kept too")
  {
    driver e;
    e.fx.inputs.filter.value = Filter::NoteOn;
    e.fx.inputs.zero_to_note_off.value = true;
    e.in(on(1, 60, 100));
    e.in(on(1, 60, 0));
    e.run();
    CHECK(driver::b(e.main) == std::vector<bytes>{{0x90, 60, 100}, {0x90, 60, 0}});
    CHECK(e.rejected.empty());
  }

  SECTION("Note running type with note off to zero")
  {
    driver e;
    e.fx.inputs.filter.value = Filter::NoteRunning;
    e.fx.inputs.mode.value = Filter::Value;
    e.fx.inputs.note_off_to_zero.value = true;
    e.in(on(1, 60, 100));
    e.in(off(1, 60));
    e.run();
    CHECK(driver::b(e.main) == std::vector<bytes>{{0x90, 60, 100}, {0x80, 60, 0}});
    CHECK(e.raw_values == std::vector<int>{100, 0});
    CHECK(e.fx.running.empty());
  }

  SECTION("Pitch bend type")
  {
    driver e;
    e.fx.inputs.filter.value = Filter::PitchBend;
    e.in(raw({0xE0, 0, 64}));
    e.in(cc(1, 1, 1));
    e.run();
    CHECK(driver::b(e.main) == std::vector<bytes>{{0xE0, 0, 64}});
    CHECK(e.raw_values == std::vector<int>{8192});
  }
}

TEST_CASE("Midi filter kinds list", "[midi_filter]")
{
  driver d;
  d.in(on(1, 60, 100));
  d.in(cc(1, 1, 1));
  d.in(raw({0xF8}));
  d.in(raw({0xF0, 1, 2, 0xF7}));
  d.in(raw({0xC0, 5}));
  d.in(raw({0xFA}));
  d.in(off(1, 60));

  auto run = [&](const char* kinds) {
    d.fx.inputs.kinds.value = kinds_of(kinds);
    auto saved = d.fx.inputs.midi.midi_messages;
    d.run();
    d.fx.inputs.midi.midi_messages = saved;
    return driver::b(d.main);
  };

  CHECK(run("note, clock") == std::vector<bytes>{{0x90, 60, 100}, {0xF8}, {0x80, 60, 0}});
  CHECK(d.rejected.size() == 4);
  CHECK(
      run("-sysex")
      == std::vector<bytes>{
          {0x90, 60, 100}, {0xB0, 1, 1}, {0xF8}, {0xC0, 5}, {0xFA}, {0x80, 60, 0}});
  CHECK(driver::b(d.rejected) == std::vector<bytes>{{0xF0, 1, 2, 0xF7}});
  CHECK(run("Realtime") == std::vector<bytes>{{0xF8}, {0xFA}});
  CHECK(run("all").size() == 7);
  CHECK(run("PC, bogus") == std::vector<bytes>{{0xC0, 5}});
  // Unknown words only: back to the Filter type (CC)
  CHECK(run("bogus") == std::vector<bytes>{{0xB0, 1, 1}});
}

TEST_CASE("Midi filter range lists and values", "[midi_filter]")
{
  driver d;
  d.fx.inputs.kinds.value = kinds_of("all");

  SECTION("Channels, notes and CCs")
  {
    d.fx.inputs.channels.value = midi_test::channels_on({1, 2, 3, 4, 6});
    d.fx.inputs.notes.value = notes_of("C4-B4");
    d.fx.inputs.ccs.value = ccs_of("7");
    d.in(on(1, 60, 100));
    d.in(on(5, 60, 100));
    d.in(on(6, 72, 100));
    d.in(cc(2, 7, 1));
    d.in(cc(2, 8, 1));
    d.in(raw({0xF8}));
    d.run();
    CHECK(
        driver::b(d.main)
        == std::vector<bytes>{{0x90, 60, 100}, {0xB1, 7, 1}, {0xF8}});
    CHECK(
        driver::b(d.rejected)
        == std::vector<bytes>{{0x94, 60, 100}, {0x95, 72, 100}, {0xB1, 8, 1}});
  }

  SECTION("Invert ranges splits a keyboard")
  {
    d.fx.inputs.notes.value = notes_of("C-1-B3");
    d.fx.inputs.drop_in_range.value = true;
    d.in(on(1, 59, 100));
    d.in(on(1, 60, 100));
    d.in(cc(1, 1, 1));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x90, 60, 100}, {0xB0, 1, 1}});
    CHECK(driver::b(d.rejected) == std::vector<bytes>{{0x90, 59, 100}});
  }

  SECTION("Values inside and outside")
  {
    d.fx.inputs.values.value = "1-63";
    d.in(cc(1, 1, 10));
    d.in(cc(1, 1, 100));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0xB0, 1, 10}});
    d.fx.inputs.value_test.value = Filter::Outside;
    d.in(cc(1, 1, 10));
    d.in(cc(1, 1, 100));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0xB0, 1, 100}});
  }

  SECTION("No channel on keeps nothing")
  {
    d.fx.inputs.channels.value = {};
    d.in(on(3, 60, 100));
    d.run();
    CHECK(d.main.empty());
  }
}

TEST_CASE("Midi filter remap", "[midi_filter]")
{
  driver d;
  d.fx.inputs.kinds.value = kinds_of("all");

  SECTION("Linear maps with rounding")
  {
    d.fx.inputs.note_map.value = {{48, 60, 60, 72}};
    d.fx.inputs.channel_map.value = {{1, 8, 9, 16}};
    d.fx.inputs.cc_map.value = {{0, 127, 0, 63}};
    d.in(on(1, 48, 100));
    d.in(on(8, 55, 100));
    d.in(on(9, 30, 100));
    d.in(cc(1, 3, 5));
    d.run();
    CHECK(
        driver::b(d.main)
        == std::vector<bytes>{
            {0x98, 60, 100}, {0x9F, 67, 100}, {0x98, 30, 100}, {0xB8, 1, 5}});
  }

  SECTION("Outside map drop sends to Rejected")
  {
    d.fx.inputs.note_map.value = {{48, 60, 60, 72}};
    d.fx.inputs.out_of_map.value = Filter::Drop;
    d.in(on(1, 30, 100));
    d.in(on(1, 50, 100));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x90, 62, 100}});
    CHECK(driver::b(d.rejected) == std::vector<bytes>{{0x90, 30, 100}});
  }

  SECTION("Note-offs follow their note-on when the settings change")
  {
    d.fx.inputs.note_map.value = {{48, 60, 60, 72}};
    d.fx.inputs.channel_map.value = {{1, 1, 2, 2}};
    d.in(on(1, 50, 100));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x91, 62, 100}});

    d.fx.inputs.note_map.value = {{48, 60, 72, 84}};
    d.fx.inputs.channel_map.value = {};
    d.fx.inputs.notes.value = notes_of("C8");
    d.in(on(1, 50, 0));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x91, 62, 0}});
    CHECK(d.rejected.empty());
  }

  SECTION("Selected note-offs go to the main output, even those of notes rejected by kind")
  {
    d.fx.inputs.kinds.value = kinds_of("noteoff");
    d.fx.inputs.note_map.value = {{0, 126, 1, 127}};
    d.in(on(1, 60, 100));
    d.in(on(1, 70, 100));
    d.run();
    CHECK(d.main.empty());
    CHECK(driver::b(d.rejected) == std::vector<bytes>{{0x90, 60, 100}, {0x90, 70, 100}});
    // The note-off keeps the mapping its note-on had.
    d.fx.inputs.note_map.value = {};
    d.in(off(1, 60));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x80, 61, 0}});
    CHECK(d.rejected.empty());
    d.fx.stop();
    d.run(0);
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x80, 71, 0}});
    CHECK(d.rejected.empty());
  }

  SECTION("Note off type: the note-offs go to the main output")
  {
    driver e;
    e.fx.inputs.filter.value = Filter::NoteOff;
    e.in(on(1, 60, 100));
    e.in(off(1, 60));
    e.in(off(1, 62));
    e.run();
    CHECK(driver::b(e.main) == std::vector<bytes>{{0x80, 60, 0}, {0x80, 62, 0}});
    CHECK(driver::b(e.rejected) == std::vector<bytes>{{0x90, 60, 100}});
  }

  SECTION("Selected note-offs of notes rejected by the criteria stay on Rejected")
  {
    d.fx.inputs.kinds.value = kinds_of("noteoff");
    d.fx.inputs.notes.value = notes_of("C8");
    d.in(on(1, 50, 100));
    d.run();
    d.fx.inputs.notes.value = notes_of("");
    d.in(off(1, 50));
    d.run();
    CHECK(d.main.empty());
    CHECK(driver::b(d.rejected) == std::vector<bytes>{{0x80, 50, 0}});
  }

  SECTION("A rejected note keeps its note-off on Rejected")
  {
    d.fx.inputs.notes.value = notes_of("C8");
    d.in(on(1, 50, 100));
    d.run();
    CHECK(driver::b(d.rejected) == std::vector<bytes>{{0x90, 50, 100}});
    d.fx.inputs.notes.value = notes_of("");
    d.in(off(1, 50));
    d.run();
    CHECK(d.main.empty());
    CHECK(driver::b(d.rejected) == std::vector<bytes>{{0x80, 50, 0}});
  }
}

TEST_CASE("Midi filter releases held notes", "[midi_filter]")
{
  driver d;
  d.fx.inputs.kinds.value = kinds_of("all");
  d.fx.inputs.note_map.value = {{0, 126, 1, 127}};
  d.fx.inputs.notes.value = notes_of("C4");

  SECTION("CC 123 sends the note-offs and passes")
  {
    d.in(on(1, 60, 100));
    d.in(on(1, 62, 100));
    d.in(on(2, 60, 100));
    d.run();
    d.in(cc(1, 123, 0, 7));
    d.run();
    CHECK(
        driver::b(d.main) == std::vector<bytes>{{0x80, 61, 0}, {0xB0, 123, 0}});
    CHECK(driver::b(d.rejected) == std::vector<bytes>{{0x80, 62, 0}});
    CHECK(d.main[0].ts == 7);
    // Channel 2 is still held and released by its own note-off
    d.in(off(2, 60));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x81, 61, 0}});
  }

  SECTION("Stop releases everything on the next tick, even an empty one")
  {
    d.in(on(1, 60, 100));
    d.in(on(1, 62, 100));
    d.run();
    d.fx.stop();
    d.run(0);
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x80, 61, 0}});
    CHECK(driver::b(d.rejected) == std::vector<bytes>{{0x80, 62, 0}});
    d.run();
    CHECK(d.main.empty());
    CHECK(d.rejected.empty());
  }

  SECTION("Repeated note-ons each get their own note-off")
  {
    d.in(on(1, 60, 100));
    d.run();
    d.fx.inputs.note_map.value = {{0, 127, 0, 127}};
    d.in(on(1, 60, 100));
    d.run();
    d.in(off(1, 60));
    d.in(off(1, 60));
    d.run();
    CHECK(driver::b(d.main) == std::vector<bytes>{{0x80, 61, 0}, {0x80, 60, 0}});
  }
}

TEST_CASE("Midi filter kind parser", "[midi_filter]")
{
  mtk::kind_set k;
  k.parse("note , CC;bend");
  CHECK(k.contains(mtk::midi_kind::note_on));
  CHECK(k.contains(mtk::midi_kind::note_off));
  CHECK(k.contains(mtk::midi_kind::control_change));
  CHECK(k.contains(mtk::midi_kind::pitch_bend));
  CHECK(!k.contains(mtk::midi_kind::clock));
  k.parse("!realtime, -note");
  CHECK(!k.contains(mtk::midi_kind::clock));
  CHECK(!k.contains(mtk::midi_kind::note_on));
  CHECK(k.contains(mtk::midi_kind::sysex));
  CHECK(k.contains(mtk::midi_kind::control_change));
  k.parse("");
  CHECK(k.empty);
}

TEST_CASE("Midi filter Types toggles", "[midi_filter]")
{
  using Kinds = Filter::ins::kinds_grid;
  STATIC_REQUIRE(Kinds::labels().size() == std::size_t(mtk::midi_kind_count - 1));
  // One toggle per kind, in the order of midi_kind
  CHECK(Kinds::labels()[int(mtk::midi_kind::program_change)] == "Program");
  CHECK(Kinds::labels()[int(mtk::midi_kind::reset)] == "Reset");

  driver d;
  d.in(on(1, 60, 100));
  d.in(raw({0xC0, 5}));
  d.in(raw({0xF0, 1, 2, 0xF7}));
  d.in(raw({0xF8}));
  // Undefined status byte: of no kind
  d.in(raw({0xF4}));
  d.in(off(1, 60));
  auto run = [&](auto kinds) {
    d.fx.inputs.kinds.value = kinds;
    auto saved = d.fx.inputs.midi.midi_messages;
    d.run();
    d.fx.inputs.midi.midi_messages = saved;
    return driver::b(d.main);
  };

  std::array<bool, mtk::midi_kind_count - 1> on_off{};
  on_off[int(mtk::midi_kind::program_change)] = true;
  on_off[int(mtk::midi_kind::sysex)] = true;
  CHECK(run(on_off) == std::vector<bytes>{{0xC0, 5}, {0xF0, 1, 2, 0xF7}});
  CHECK(driver::b(d.rejected)
        == std::vector<bytes>{{0x90, 60, 100}, {0xF8}, {0xF4}, {0x80, 60, 0}});

  // Every toggle on keeps what has no kind too, as "all" did
  on_off.fill(true);
  CHECK(run(on_off).size() == 6);
  // All but one: what has no kind is rejected
  on_off[int(mtk::midi_kind::clock)] = false;
  CHECK(
      run(on_off)
      == std::vector<bytes>{{0x90, 60, 100}, {0xC0, 5}, {0xF0, 1, 2, 0xF7}, {0x80, 60, 0}});

  // None on: the Filter type (CC by default) decides
  on_off.fill(false);
  CHECK(run(on_off).empty());
  d.fx.inputs.filter.value = Filter::NoteAny;
  CHECK(run(on_off) == std::vector<bytes>{{0x90, 60, 100}, {0x80, 60, 0}});
}

TEST_CASE("Midi filter Notes and CCs rows", "[midi_filter]")
{
  driver d;
  d.fx.inputs.kinds.value.fill(true);
  // Two ranges, one given backwards
  d.fx.inputs.notes.value = {{60, 62}, {72, 70}};
  d.fx.inputs.ccs.value = {{64, 64}};
  for(int n : {59, 60, 62, 63, 69, 70, 71, 72, 73})
    d.in(on(1, n, 100));
  for(int c : {1, 64, 65})
    d.in(cc(1, c, 10));
  d.run();
  CHECK(
      driver::b(d.main)
      == std::vector<bytes>{
          {0x90, 60, 100}, {0x90, 62, 100}, {0x90, 70, 100}, {0x90, 71, 100},
          {0x90, 72, 100}, {0xB0, 64, 10}});

  // No rows: every note and controller
  d.fx.inputs.notes.value.clear();
  d.fx.inputs.ccs.value.clear();
  d.in(on(1, 20, 100));
  d.in(cc(1, 1, 10));
  d.run();
  CHECK(driver::b(d.main) == std::vector<bytes>{{0x90, 20, 100}, {0xB0, 1, 10}});
}

TEST_CASE("Midi filter: the texts Types, Notes and CCs were", "[midi_filter][upgrade]")
{
  using Kinds = Filter::ins::kinds_grid;
  using Notes = Filter::ins::notes_list;
  using CCs = Filter::ins::ccs_list;
  auto bools = [](const ossia::value& v) {
    std::vector<bool> res;
    for(const auto& b : *v.target<std::vector<ossia::value>>())
      res.push_back(*b.target<bool>());
    return res;
  };
  auto rows = [](const ossia::value& v) {
    std::vector<std::pair<int, int>> res;
    for(const auto& r : *v.target<std::vector<ossia::value>>())
    {
      const auto& l = *r.target<std::vector<ossia::value>>();
      res.emplace_back(*l[0].target<int>(), *l[1].target<int>());
    }
    return res;
  };

  {
    const auto v = bools(Kinds::upgrade_value(std::string{"-sysex, -clock"}));
    REQUIRE(v.size() == 18);
    for(int i = 0; i < 18; i++)
      CHECK(v[i] == (i != int(mtk::midi_kind::sysex) && i != int(mtk::midi_kind::clock)));
  }
  {
    const auto v = bools(Kinds::upgrade_value(std::string{"transport, cc"}));
    std::vector<bool> expected(18, false);
    for(auto k : {mtk::midi_kind::start, mtk::midi_kind::continue_, mtk::midi_kind::stop,
                  mtk::midi_kind::control_change})
      expected[int(k)] = true;
    CHECK(v == expected);
  }
  // Empty: no toggle on, the Filter type decides as it did
  CHECK(bools(Kinds::upgrade_value(std::string{})) == std::vector<bool>(18, false));
  // What is already toggles is kept
  const ossia::value grid = std::vector<ossia::value>(18, ossia::value{true});
  CHECK(Kinds::upgrade_value(grid) == grid);

  CHECK(
      rows(Notes::upgrade_value(std::string{"C-1-B2, C4, F#5-60"}))
      == std::vector<std::pair<int, int>>{{0, 47}, {60, 60}, {60, 78}});
  CHECK(
      rows(CCs::upgrade_value(std::string{"1, 7, 64-69"}))
      == std::vector<std::pair<int, int>>{{1, 1}, {7, 7}, {64, 69}});
  // A text that did not parse restricted nothing: no rows
  CHECK(rows(Notes::upgrade_value(std::string{"C4-"})).empty());
  CHECK(rows(CCs::upgrade_value(std::string{"200"})).empty());
  // Note names are only notes
  CHECK(rows(CCs::upgrade_value(std::string{"C4"})).empty());
}
