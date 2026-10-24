// Chord (Nodes::Chord::Node): the chord table, harmonizer, chance and
// velocity per voice, strum, chord memory and MPE, and the common checklist of
// the MIDI objects: every note-on gets one note-off, whatever the controls do.

#include <Fx/Chord.hpp>

#include <avnd/binding/ossia/from_value.hpp>

#include "MidiChannelMask.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <map>
#include <new>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{
std::atomic_bool g_count_allocations{false};
std::atomic<int> g_allocations{0};
}

void* operator new(std::size_t n)
{
  if(g_count_allocations.load(std::memory_order_relaxed))
    g_allocations++;
  if(void* p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc{};
}
void operator delete(void* p) noexcept
{
  std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
  std::free(p);
}

namespace
{
using Chord = Nodes::Chord::Node;

libremidi::message note_on(int chan, int pitch, int vel, int64_t ts = 0)
{
  auto m = libremidi::channel_events::note_on(chan, pitch, vel);
  m.timestamp = ts;
  return m;
}
libremidi::message note_off(int chan, int pitch, int64_t ts = 0)
{
  auto m = libremidi::channel_events::note_off(chan, pitch, 0);
  m.timestamp = ts;
  return m;
}

struct Listener
{
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;

  void feed(const libremidi::message& m, int frames)
  {
    if(frames > 0 && (m.timestamp < 0 || m.timestamp >= frames))
      errors.push_back("timestamp " + std::to_string(m.timestamp));
    if(frames == 0 && m.timestamp != 0)
      errors.push_back("timestamp in empty tick " + std::to_string(m.timestamp));
    const auto type = m.get_message_type();
    if(type != libremidi::message_type::NOTE_ON && type != libremidi::message_type::NOTE_OFF)
      return;
    const int chan = m.get_channel();
    const int pitch = m.bytes[1];
    const bool on = type == libremidi::message_type::NOTE_ON && m.bytes[2] > 0;
    auto& n = sounding[{chan, pitch}];
    if(on)
    {
      if(n > 0)
        errors.push_back("started twice: ch " + std::to_string(chan) + " " + std::to_string(pitch));
      n++;
    }
    else
    {
      if(n == 0)
        errors.push_back(
            "stopped while silent: ch " + std::to_string(chan) + " " + std::to_string(pitch));
      else
        n--;
    }
  }

  int still_sounding() const
  {
    int total = 0;
    for(auto& [k, n] : sounding)
      total += n;
    return total;
  }
};

struct Rig
{
  Chord node;
  Listener out;
  std::vector<libremidi::message> last;
  //! Absolute frame of each message of the last tick.
  std::vector<int64_t> last_at;
  int64_t now{};
  std::string learned;

  Rig()
  {
    node.prepare(halp::setup{.rate = 48000.});
    node.inputs.seed.value = 1;
    node.inputs.memory.update_controller = [this](std::string_view s) { learned = std::string(s); };
  }

  const std::vector<libremidi::message>& tick(std::vector<libremidi::message> in = {}, int frames = 64)
  {
    node.inputs.midi.midi_messages.assign(in.begin(), in.end());
    node.outputs.midi.midi_messages.clear();
    halp::tick_musical t;
    t.frames = frames;
    node(t);
    last.assign(node.outputs.midi.begin(), node.outputs.midi.end());
    last_at.clear();
    for(auto& m : last)
    {
      out.feed(m, frames);
      last_at.push_back(now + m.timestamp);
    }
    now += frames;
    return last;
  }

  //! Ticks until `frames` have elapsed, collecting note-ons with their dates.
  std::vector<std::pair<int64_t, libremidi::message>> run(int64_t frames)
  {
    std::vector<std::pair<int64_t, libremidi::message>> r;
    const int64_t end = now + frames;
    while(now < end)
    {
      tick();
      for(std::size_t i = 0; i < last.size(); i++)
        r.push_back({last_at[i], last[i]});
    }
    return r;
  }
};

std::vector<int> pitches_on(const std::vector<libremidi::message>& v)
{
  std::vector<int> r;
  for(auto& m : v)
    if(mtk::is_note_on(m))
      r.push_back(m.bytes[1]);
  return r;
}

std::vector<int> voices(Chord& c, int pitch = 60)
{
  std::array<int, Chord::max_voices> v{};
  const int n = c.voices(pitch, v);
  return {v.begin(), v.begin() + n};
}

constexpr auto chord_names = decltype(Chord{}.inputs.chord)::range{};
}

TEST_CASE("Chord: the six original types keep their names and notes", "[fx][midi][chord]")
{
  const std::vector<std::vector<int>> expected{
      {0, 4, 7, 11, 12}, {0, 3, 7, 10, 12}, {0, 2, 7, 9, 12},
      {0, 5, 7, 9, 12},  {0, 3, 6, 9, 12},  {0, 4, 8, 10, 12}};
  const char* names[] = {"Maj", "Min", "Sus2", "Sus4", "Dim", "Aug"};
  for(int t = 0; t < 6; t++)
  {
    CHECK(chord_names.values[t] == names[t]);
    Rig r;
    r.node.inputs.chord.value = Chord::Chord(t);
    r.node.inputs.num.value = 5;
    auto& on = r.tick({note_on(3, 60, 99, 5)});
    REQUIRE(on.size() == 5);
    for(int i = 0; i < 5; i++)
    {
      CHECK(on[i].bytes[0] == 0x92);
      CHECK(on[i].bytes[1] == 60 + expected[t][i]);
      CHECK(on[i].bytes[2] == 99);
      CHECK(on[i].timestamp == 5);
    }
    auto& off = r.tick({note_off(3, 60, 9)});
    CHECK(off.size() == 5);
    CHECK(r.out.still_sounding() == 0);
    CHECK(r.out.errors.empty());
  }

  // Default: three notes of Maj.
  Rig r;
  CHECK(pitches_on(r.tick({note_on(1, 60, 100)})) == std::vector<int>{60, 64, 67});
  // Notes past 127 are left out.
  r.node.inputs.num.value = 5;
  CHECK(pitches_on(r.tick({note_on(1, 120, 100)})) == std::vector<int>{120, 124, 127});
}

TEST_CASE("Chord: the table follows mtk::chord_types", "[fx][midi][chord]")
{
  static_assert(std::size(chord_names.values) == 43);
  const std::map<std::string_view, std::string_view> renamed{
      {"M", "maj triad"},   {"m", "min triad"},   {"dim", "dim triad"}, {"aug", "aug triad"},
      {"sus2", "sus2 triad"}, {"sus4", "sus4 triad"}, {"M7+5", "maj7+5"},
      {"5", "Power"},         {"6", "maj6"},          {"7", "Dom7"},
      {"7-5", "Dom7-5"},      {"7+5", "Dom7+5"},      {"7sus4", "Dom7sus4"},
      {"69", "maj6/9"},       {"9", "Dom9"},          {"9sus4", "Dom9sus4"},
      {"7-9", "Dom7-9"},      {"7+11", "Dom7+11"}};
  for(int i = 0; i < mtk::chord_type_count; i++)
  {
    const auto& t = mtk::chord_types[i];
    auto it = renamed.find(t.name);
    CHECK(chord_names.values[6 + i] == (it != renamed.end() ? it->second : t.name));

    Chord c;
    c.inputs.chord.value = Chord::Chord(6 + i);
    c.inputs.num.value = 5;
    std::vector<int> expected(t.notes().begin(), t.notes().end());
    CHECK(voices(c) == expected);
  }
  // No two entries share a name, even ignoring case.
  for(int i = 0; i < 43; i++)
    for(int j = i + 1; j < 43; j++)
    {
      std::string a{chord_names.values[i]}, b{chord_names.values[j]};
      for(auto& ch : a)
        ch = char(std::tolower(ch));
      for(auto& ch : b)
        ch = char(std::tolower(ch));
      CHECK(a != b);
    }

  // An integer is an index: no name could take it for itself.
  for(int i = 0; i < 43; i++)
    CHECK(!std::isdigit((unsigned char)chord_names.values[i][0]));
  for(int i : {5, 6, 7, 9, 24, 25})
  {
    Chord n;
    oscr::from_ossia_value(n.inputs.chord, ossia::value{i}, n.inputs.chord.value);
    CHECK(int(n.inputs.chord.value) == i);
    oscr::from_ossia_value(
        n.inputs.chord, ossia::value{std::to_string(i)}, n.inputs.chord.value);
    CHECK(int(n.inputs.chord.value) == i);
  }

  // Num. Notes caps the count; a chord with fewer notes plays them all.
  Chord c;
  c.inputs.chord.value = Chord::Chord(6 + 28); // dim7
  REQUIRE(chord_names.values[6 + 28] == "dim7");
  CHECK(voices(c) == std::vector<int>{0, 3, 6});
  c.inputs.num.value = 5;
  CHECK(voices(c) == std::vector<int>{0, 3, 6, 9});
}

TEST_CASE("Chord: harmonizer intervals, root and inversions", "[fx][midi][chord]")
{
  Chord c;
  c.inputs.mode.value = Chord::Harmonizer;
  c.inputs.intervals.value = {4, 7, 4, 0};
  CHECK(voices(c) == std::vector<int>{0, 4, 7});
  c.inputs.root.value = false;
  CHECK(voices(c) == std::vector<int>{4, 7, 0});
  c.inputs.intervals.value = {-12, 19};
  CHECK(voices(c) == std::vector<int>{-12, 19});
  c.inputs.intervals.value = {};
  CHECK(voices(c).empty());

  c.inputs.root.value = true;
  c.inputs.intervals.value = {4, 7};
  c.inputs.inversion.value = 1;
  CHECK(voices(c) == std::vector<int>{12, 4, 7});
  c.inputs.inversion.value = 2;
  CHECK(voices(c) == std::vector<int>{12, 16, 7});
  // Past the number of notes: octave inversions.
  c.inputs.inversion.value = 3;
  CHECK(voices(c) == std::vector<int>{12, 16, 19});
  c.inputs.inversion.value = 4;
  CHECK(voices(c) == std::vector<int>{24, 16, 19});
  c.inputs.inversion.value = -1;
  CHECK(voices(c) == std::vector<int>{0, 4, -5});

  // An inversion that makes two voices meet keeps one of them.
  c.inputs.mode.value = Chord::Table;
  c.inputs.num.value = 5;
  c.inputs.inversion.value = 1;
  CHECK(voices(c) == std::vector<int>{12, 4, 7, 11});

  // Played: the inversion sounds, in pitch order.
  Rig r;
  r.node.inputs.mode.value = Chord::Harmonizer;
  r.node.inputs.intervals.value = {4, 7};
  r.node.inputs.inversion.value = 1;
  CHECK(pitches_on(r.tick({note_on(1, 60, 100)})) == std::vector<int>{64, 67, 72});
  r.tick({note_off(1, 60)});
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Chord: chance and velocity per voice", "[fx][midi][chord]")
{
  Rig r;
  r.node.inputs.chance.value = {1.f, 0.f};
  CHECK(pitches_on(r.tick({note_on(1, 60, 100)})) == std::vector<int>{60});
  r.tick({note_off(1, 60)});

  r.node.inputs.chance.value = {1.f};
  r.node.inputs.velocity.value = {1.f, 0.5f, 2.f};
  auto& on = r.tick({note_on(1, 60, 100)});
  REQUIRE(on.size() == 3);
  CHECK(on[0].bytes[2] == 100);
  CHECK(on[1].bytes[2] == 50);
  CHECK(on[2].bytes[2] == 127);
  r.tick({note_off(1, 60)});

  // A seed gives the same take; a stop restarts it.
  auto take = [](int seed) {
    Rig r;
    r.node.inputs.seed.value = seed;
    r.node.inputs.chance.value = {0.5f};
    r.node.inputs.num.value = 5;
    std::vector<int> played;
    for(int i = 0; i < 40; i++)
    {
      for(int p : pitches_on(r.tick({note_on(1, 60, 100)})))
        played.push_back(p);
      played.push_back(-1);
      r.tick({note_off(1, 60)});
    }
    CHECK(r.out.errors.empty());
    CHECK(r.out.still_sounding() == 0);
    return played;
  };
  const auto a = take(7);
  CHECK(a == take(7));
  CHECK(a != take(8));
  int notes = 0;
  for(int p : a)
    notes += p >= 0;
  CHECK(notes > 40);
  CHECK(notes < 160);
}

TEST_CASE("Chord: strum time, direction, tension and crescendo", "[fx][midi][chord]")
{
  auto strum = [](auto setup) {
    Rig r;
    r.node.inputs.strum.value = 0.01f; // 480 frames at 48 kHz
    setup(r.node);
    r.tick({note_on(1, 60, 100, 10)});
    std::vector<std::pair<int64_t, libremidi::message>> all;
    for(std::size_t i = 0; i < r.last.size(); i++)
      all.push_back({r.last_at[i], r.last[i]});
    for(auto& e : r.run(2048))
      all.push_back(e);
    r.tick({note_off(1, 60)});
    CHECK(r.out.errors.empty());
    CHECK(r.out.still_sounding() == 0);
    return all;
  };

  auto up = strum([](Chord&) {});
  REQUIRE(up.size() == 3);
  CHECK(up[0].first == 10);
  CHECK(up[0].second.bytes[1] == 60);
  CHECK(up[1].first == 490);
  CHECK(up[1].second.bytes[1] == 64);
  CHECK(up[2].first == 970);
  CHECK(up[2].second.bytes[1] == 67);

  auto down = strum([](Chord& c) { c.inputs.strum_direction.value = Chord::Down; });
  REQUIRE(down.size() == 3);
  CHECK(down[0].second.bytes[1] == 67);
  CHECK(down[2].second.bytes[1] == 60);

  // Positive tension: the gaps grow.
  auto slow = strum([](Chord& c) { c.inputs.tension.value = 1.f; });
  REQUIRE(slow.size() == 3);
  CHECK(slow[1].first == 10 + 960 / 16);
  CHECK(slow[2].first == 970);

  auto cresc = strum([](Chord& c) { c.inputs.crescendo.value = 0.5f; c.inputs.velocity.value = {0.8f}; });
  REQUIRE(cresc.size() == 3);
  CHECK(cresc[0].second.bytes[2] == 80);
  CHECK(cresc[1].second.bytes[2] == 100);
  CHECK(cresc[2].second.bytes[2] == 120);

  // Alternate: up, then down.
  Rig r;
  r.node.inputs.strum.value = 0.001f;
  r.node.inputs.strum_direction.value = Chord::Alternate;
  r.tick({note_on(1, 60, 100)});
  std::vector<int> first = pitches_on(r.last);
  for(auto& e : r.run(256))
    if(mtk::is_note_on(e.second))
      first.push_back(e.second.bytes[1]);
  r.tick({note_off(1, 60)});
  r.tick({note_on(1, 60, 100)});
  std::vector<int> second = pitches_on(r.last);
  for(auto& e : r.run(256))
    if(mtk::is_note_on(e.second))
      second.push_back(e.second.bytes[1]);
  r.tick({note_off(1, 60)});
  CHECK(first == std::vector<int>{60, 64, 67});
  CHECK(second == std::vector<int>{67, 64, 60});
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Chord: releasing a key mid-strum cancels the notes still to come", "[fx][midi][chord]")
{
  Rig r;
  r.node.inputs.strum.value = 0.01f;
  r.tick({note_on(1, 60, 100)});
  r.run(600); // 60 and 64 have started
  CHECK(r.out.still_sounding() == 2);
  auto& off = r.tick({note_off(1, 60, 3)});
  CHECK(off.size() == 2);
  r.run(2048);
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());

  // A note-off in the same tick as a strummed note keeps their order.
  Rig q;
  q.node.inputs.strum.value = 0.001f; // 48 frames
  auto& both = q.tick({note_on(1, 60, 100, 0), note_off(1, 60, 60)});
  REQUIRE(both.size() == 4);
  CHECK(mtk::is_note_on(both[0]));
  CHECK(mtk::is_note_on(both[1]));
  CHECK(both[1].timestamp == 48);
  CHECK(mtk::is_note_off(both[2]));
  CHECK(mtk::is_note_off(both[3]));
  q.run(256);
  CHECK(q.out.still_sounding() == 0);
  CHECK(q.out.errors.empty());

  // Pressed again mid-strum: the previous chord ends, its pending notes too.
  Rig p;
  p.node.inputs.strum.value = 0.01f;
  p.tick({note_on(1, 60, 100)});
  p.tick({note_on(1, 60, 90)});
  p.run(4096);
  p.tick({note_off(1, 60)});
  CHECK(p.out.still_sounding() == 0);
  CHECK(p.out.errors.empty());
}

TEST_CASE("Chord: chord memory, learned and written", "[fx][midi][chord]")
{
  Rig r;
  r.node.inputs.learn.value = true;
  // While learning, notes pass through.
  CHECK(pitches_on(r.tick({note_on(1, 60, 100), note_on(1, 64, 100)})) == std::vector<int>{60, 64});
  r.tick({note_on(1, 67, 100)});
  r.tick({note_off(1, 60), note_off(1, 64)});
  r.tick({note_off(1, 67)});
  CHECK(r.learned.empty());
  // The next key alone is the trigger.
  r.tick({note_on(1, 48, 100)});
  CHECK(r.learned == "48: 12 16 19");
  r.tick({note_off(1, 48)});
  // A single key with nothing captured learns nothing.
  r.tick({note_on(1, 50, 100)});
  r.tick({note_off(1, 50)});
  CHECK(r.learned == "48: 12 16 19");
  // A second chord adds to the table.
  r.tick({note_on(1, 62, 100), note_on(1, 65, 100), note_on(1, 69, 100), note_on(1, 72, 100)});
  r.tick({note_off(1, 62), note_off(1, 65), note_off(1, 69), note_off(1, 72)});
  r.tick({note_on(1, 50, 100)});
  r.tick({note_off(1, 50)});
  CHECK(r.learned == "48: 12 16 19; 50: 12 15 19 22");

  r.node.inputs.learn.value = false;
  r.node.inputs.mode.value = Chord::Memory;
  CHECK(pitches_on(r.tick({note_on(1, 48, 100)})) == std::vector<int>{60, 64, 67});
  CHECK(pitches_on(r.tick({note_on(1, 50, 100)})) == std::vector<int>{62, 65, 69, 72});
  // A key with no chord passes through.
  CHECK(pitches_on(r.tick({note_on(1, 40, 100)})) == std::vector<int>{40});
  r.tick({note_off(1, 48), note_off(1, 50), note_off(1, 40)});
  CHECK(r.out.still_sounding() == 0);

  // The text comes back from the control: same table.
  r.node.inputs.memory.value = r.learned;
  CHECK(pitches_on(r.tick({note_on(1, 48, 100)})) == std::vector<int>{60, 64, 67});
  r.tick({note_off(1, 48)});
  CHECK(r.out.errors.empty());
}

TEST_CASE("Chord: learning a chord does not allocate", "[fx][midi][chord]")
{
  Rig r;
  // What the binding does with the text: copy it into storage made beforehand.
  std::array<char, Chord::max_memory_text> sent{};
  std::size_t sent_size = 0;
  r.node.inputs.memory.update_controller = [&](std::string_view s) {
    std::copy(s.begin(), s.end(), sent.begin());
    sent_size = s.size();
  };
  r.node.inputs.learn.value = true;

  auto learn = [&](std::vector<libremidi::message> chord, int key) {
    std::vector<libremidi::message> offs;
    for(auto& m : chord)
      offs.push_back(note_off(1, m.bytes[1]));
    r.tick(chord);
    r.tick(offs);
    std::vector<libremidi::message> trigger{note_on(1, key, 100)};
    r.node.inputs.midi.midi_messages.assign(trigger.begin(), trigger.end());
    r.node.outputs.midi.midi_messages.clear();
    halp::tick_musical t;
    t.frames = 64;
    g_allocations = 0;
    g_count_allocations = true;
    r.node(t);
    g_count_allocations = false;
    CHECK(g_allocations == 0);
    r.node.outputs.midi.midi_messages.clear();
    r.tick({note_off(1, key)});
  };

  // Past the size of a small string from the second chord on.
  learn({note_on(1, 60, 100), note_on(1, 64, 100), note_on(1, 67, 100)}, 48);
  learn({note_on(1, 62, 100), note_on(1, 65, 100), note_on(1, 69, 100)}, 50);
  learn({note_on(1, 64, 100), note_on(1, 67, 100), note_on(1, 71, 100)}, 52);
  CHECK(
      std::string_view(sent.data(), sent_size)
      == "48: 12 16 19; 50: 12 15 19; 52: 12 15 19");
}

TEST_CASE("Chord: chord memory text", "[fx][midi][chord]")
{
  std::array<Chord::learned_chord, 128> t{};
  Chord::parse_memory("60: 0 4 7; C3: 12, 15 19\n 62:0 3 7 10 ; bad; 70 1 2; 200: 1; 61:", t);
  CHECK(t[60].count == 3);
  CHECK(t[48].count == 3);
  CHECK(t[48].offsets[1] == 15);
  CHECK(t[62].count == 4);
  CHECK(t[70].count == 0);
  CHECK(t[61].count == 0);
  int total = 0;
  for(auto& c : t)
    total += c.count > 0;
  CHECK(total == 3);

  char buf[256];
  const auto n = Chord::print_memory(t, buf, sizeof(buf));
  CHECK(std::string_view(buf, n) == "48: 12 15 19; 60: 0 4 7; 62: 0 3 7 10");

  Chord c;
  c.inputs.mode.value = Chord::Memory;
  c.inputs.memory.value = "60: 0 4 7; C3: -12 0";
  CHECK(voices(c, 60) == std::vector<int>{0, 4, 7});
  CHECK(voices(c, 48) == std::vector<int>{-12, 0});
  CHECK(voices(c, 61) == std::vector<int>{0});
}

TEST_CASE("Chord: MPE voices on their own channels", "[fx][midi][chord]")
{
  Rig r;
  r.node.inputs.mpe.value = true;
  r.node.inputs.mpe_channels.value = midi_test::channels_on({2, 3, 4});
  auto& on = r.tick({note_on(1, 60, 100)});
  REQUIRE(on.size() == 3);
  CHECK(on[0].get_channel() == 2);
  CHECK(on[1].get_channel() == 3);
  CHECK(on[2].get_channel() == 4);

  // Per-note expression of the source note reaches every voice.
  auto bend = libremidi::channel_events::pitch_bend(1, 10000);
  auto& b = r.tick({bend});
  REQUIRE(b.size() == 3);
  for(int i = 0; i < 3; i++)
  {
    CHECK(b[i].get_message_type() == libremidi::message_type::PITCH_BEND);
    CHECK(b[i].get_channel() == 2 + i);
    CHECK(b[i].bytes[1] == bend.bytes[1]);
    CHECK(b[i].bytes[2] == bend.bytes[2]);
  }
  auto& p = r.tick({libremidi::channel_events::aftertouch(1, 50)});
  CHECK(p.size() == 3);
  auto& pp = r.tick({libremidi::channel_events::poly_pressure(1, 60, 70)});
  REQUIRE(pp.size() == 3);
  CHECK(pp[1].bytes[1] == 64);
  CHECK(pp[1].get_channel() == 3);
  auto& timbre = r.tick({libremidi::channel_events::control_change(1, 74, 20)});
  CHECK(timbre.size() == 3);
  // Expression of a channel with no held note passes through.
  auto& other = r.tick({libremidi::channel_events::pitch_bend(5, 100)});
  REQUIRE(other.size() == 1);
  CHECK(other[0].get_channel() == 5);

  // No free channel: the new note's voices are dropped.
  CHECK(r.tick({note_on(6, 70, 100)}).empty());
  r.tick({note_off(6, 70)});
  r.tick({note_off(1, 60)});
  CHECK(r.out.still_sounding() == 0);

  // The voice channels still carry the old bend: a new note re-centres them.
  CHECK(r.tick({libremidi::channel_events::pitch_bend(1, 8192)}).size() == 1);
  auto& again = r.tick({note_on(1, 62, 100)});
  REQUIRE(again.size() == 6);
  CHECK(again[0].get_message_type() == libremidi::message_type::PITCH_BEND);
  CHECK(mtk::is_note_on(again[1]));
  r.tick({note_off(1, 62)});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Chord: all notes off, pass through, stop", "[fx][midi][chord]")
{
  Rig r;
  r.tick({note_on(1, 60, 100), note_on(2, 62, 100)});
  auto& panic = r.tick({libremidi::channel_events::control_change(1, 123, 0)});
  REQUIRE(panic.size() == 4);
  CHECK(panic[3].bytes[1] == 123);
  CHECK(r.out.still_sounding() == 3);
  CHECK(r.tick({note_off(1, 60)}).empty()); // nothing left to end

  // Other messages keep their order and timestamps.
  auto cc = libremidi::channel_events::control_change(1, 7, 64);
  cc.timestamp = 5;
  auto pc = libremidi::channel_events::program_change(1, 3);
  pc.timestamp = 6;
  auto& pass = r.tick({cc, pc});
  REQUIRE(pass.size() == 2);
  CHECK(pass[0].bytes == cc.bytes);
  CHECK(pass[0].timestamp == 5);
  CHECK(pass[1].bytes == pc.bytes);
  CHECK(pass[1].timestamp == 6);

  // Stop: note-offs in a tick with no frames, pending notes dropped.
  r.node.inputs.strum.value = 0.01f;
  r.tick({note_on(3, 50, 100)});
  r.node.stop();
  r.tick({}, 0);
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.run(4096).empty());
  CHECK(r.out.errors.empty());

  // A note-on with velocity 0 is a note-off.
  r.node.inputs.strum.value = 0.f;
  r.tick({note_on(1, 60, 100)});
  r.tick({note_on(1, 60, 0)});
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Chord: random playing and control changes never leave a note behind", "[fx][midi][chord]")
{
  for(unsigned seed = 1; seed <= 20; seed++)
  {
    std::mt19937 rng{seed};
    auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>{lo, hi}(rng); };
    Rig r;
    std::map<std::pair<int, int>, bool> held;
    for(int step = 0; step < 400; step++)
    {
      std::vector<libremidi::message> in;
      for(int i = pick(0, 4); i > 0; i--)
      {
        const int ch = pick(1, 3), p = pick(0, 127);
        switch(pick(0, 4))
        {
          case 0:
          case 1:
            in.push_back(note_on(ch, p, pick(1, 127), pick(0, 63)));
            held[{ch, p}] = true;
            break;
          case 2:
            in.push_back(note_off(ch, p, pick(0, 63)));
            held[{ch, p}] = false;
            break;
          case 3:
            in.push_back(note_on(ch, p, 0, pick(0, 63)));
            held[{ch, p}] = false;
            break;
          case 4:
            in.push_back(libremidi::channel_events::pitch_bend(ch, pick(0, 16383)));
            break;
        }
      }
      std::sort(in.begin(), in.end(), [](auto& a, auto& b) { return a.timestamp < b.timestamp; });
      auto& n = r.node.inputs;
      switch(pick(0, 30))
      {
        case 0: n.chord.value = Chord::Chord(pick(0, 42)); break;
        case 1: n.num.value = pick(1, 5); break;
        case 2: n.mode.value = Chord::PlayMode(pick(0, 2)); break;
        case 3:
          if(pick(0, 1))
            n.intervals.value = {4, 7, 11};
          else
            n.intervals.value = {-12, 3, 19, 24};
          break;
        case 4: n.inversion.value = pick(-8, 8); break;
        case 5:
          if(pick(0, 1))
            n.chance.value = {1.f, 0.5f};
          else
            n.chance.value = {0.3f};
          break;
        case 6: n.strum.value = pick(0, 1) ? 0.f : 0.003f; break;
        case 7: n.strum_direction.value = Chord::StrumDirection(pick(0, 2)); break;
        case 8: n.mpe.value = !n.mpe.value; break;
        case 9: n.memory.value = "60: 0 4 7; 62: -12 0 12 24"; break;
        case 10: n.learn.value = !n.learn.value; break;
        case 11: r.node.stop(); break;
        case 12: n.root.value = !n.root.value; break;
        default: break;
      }
      r.tick(std::move(in), pick(0, 20) == 0 ? 0 : 64);
    }
    std::vector<libremidi::message> release;
    for(auto& [k, h] : held)
      if(h)
        release.push_back(note_off(k.first, k.second));
    r.tick(std::move(release));
    r.run(4096);

    INFO("seed " << seed << ": " << r.out.errors.size() << " errors, first: "
                 << (r.out.errors.empty() ? "" : r.out.errors[0]));
    CHECK(r.out.errors.empty());
    CHECK(r.out.still_sounding() == 0);
  }
}
