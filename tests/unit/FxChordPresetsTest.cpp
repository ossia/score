// The Chord presets of the default package: each one loads onto the object and
// plays what its name says, with every note-on paired with one note-off.

#include "MidiPresetHarness.hpp"

#include <Fx/Chord.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Chord = Nodes::Chord::Node;
constexpr int frames = 512;
constexpr double rate = 48000.;

const std::vector<midi_presets::preset>& presets()
{
  static const auto p = midi_presets::load("Chord", "F0904279-EA26-48DB-B0DF-F68FE3091DA1");
  return p;
}

const midi_presets::preset* find(const char* name)
{
  for(auto& p : presets())
    if(p.name == name)
      return &p;
  return nullptr;
}

struct played
{
  int64_t at{};
  int channel{};
  int pitch{};
  int velocity{};
};

struct Rig
{
  Chord node;
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;
  std::vector<played> ons;
  std::vector<libremidi::message> others;
  int64_t now{};

  explicit Rig(const char* name)
  {
    node.prepare(halp::setup{.rate = rate});
    const auto* p = find(name);
    if(!p)
    {
      errors.push_back(std::string("missing preset ") + name);
      return;
    }
    const auto e = midi_presets::apply(node.inputs, *p);
    for(auto& s : e)
      errors.push_back(s.toStdString());
  }

  void tick(std::vector<libremidi::message> in = {})
  {
    node.inputs.midi.midi_messages.assign(in.begin(), in.end());
    node.outputs.midi.midi_messages.clear();
    halp::tick_musical t;
    t.frames = frames;
    node(t);
    for(auto& m : node.outputs.midi.midi_messages)
    {
      if(m.timestamp < 0 || m.timestamp >= frames)
        errors.push_back("timestamp " + std::to_string(m.timestamp));
      if(mtk::is_note_on(m))
      {
        auto& n = sounding[{m.get_channel(), m.bytes[1]}];
        if(n++ > 0)
          errors.push_back("started twice " + std::to_string(m.bytes[1]));
        ons.push_back({now + m.timestamp, m.get_channel(), m.bytes[1], m.bytes[2]});
      }
      else if(mtk::is_note_off(m))
      {
        auto& n = sounding[{m.get_channel(), m.bytes[1]}];
        if(n == 0)
          errors.push_back("stopped while silent " + std::to_string(m.bytes[1]));
        else
          n--;
      }
      else
      {
        others.push_back(m);
      }
    }
    now += frames;
  }

  void run(int ticks)
  {
    for(int i = 0; i < ticks; i++)
      tick();
  }

  //! Plays one key for a second and releases it; returns its note-ons.
  std::vector<played> strike(int pitch, int vel = 100, int ch = 1)
  {
    const auto first = ons.size();
    tick({libremidi::channel_events::note_on(ch, pitch, vel)});
    run(int(rate / frames));
    tick({libremidi::channel_events::note_off(ch, pitch, 0)});
    run(4);
    return {ons.begin() + first, ons.end()};
  }

  int still_sounding() const
  {
    int total = 0;
    for(auto& [k, n] : sounding)
      total += n;
    return total;
  }
};

std::set<int> pitches(const std::vector<played>& v)
{
  std::set<int> r;
  for(auto& p : v)
    r.insert(p.pitch);
  return r;
}

int velocity_of(const std::vector<played>& v, int pitch)
{
  for(auto& p : v)
    if(p.pitch == pitch)
      return p.velocity;
  return -1;
}

void check_clean(const Rig& r)
{
  INFO((r.errors.empty() ? std::string{} : r.errors.front()));
  CHECK(r.errors.empty());
  CHECK(r.still_sounding() == 0);
}

void check_chord(const char* name, int key, std::set<int> expected)
{
  INFO(name);
  Rig r{name};
  auto v = r.strike(key);
  CHECK(pitches(v) == expected);
  CHECK(v.size() == expected.size());
  // No strum: everything starts with the key
  for(auto& p : v)
    CHECK(p.at == 0);
  check_clean(r);
}
}

TEST_CASE("Chord presets: every preset loads", "[presets][chord]")
{
  REQUIRE(presets().size() >= 6);
  for(auto& p : presets())
  {
    INFO(p.name.toStdString());
    Chord c;
    const auto errors = midi_presets::apply(c.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
  }
}

TEST_CASE("Chord presets: chord types", "[presets][chord]")
{
  check_chord("Major triad", 60, {60, 64, 67});
  check_chord("Minor triad", 60, {60, 63, 67});
  check_chord("Major seventh", 60, {60, 64, 67, 71});
  check_chord("Dominant seventh", 60, {60, 64, 67, 70});
  check_chord("Minor seventh", 60, {60, 63, 67, 70});
  check_chord("Dominant ninth", 60, {60, 64, 67, 70, 74});
  check_chord("Power chord", 60, {60, 67});
  check_chord("Power chord", 40, {40, 47});
}

TEST_CASE("Chord presets: Sus4 pad", "[presets][chord]")
{
  Rig r{"Sus4 pad"};
  auto v = r.strike(62, 100);
  CHECK(pitches(v) == std::set<int>{62, 67, 69});
  CHECK(velocity_of(v, 62) == 90);
  CHECK(velocity_of(v, 67) == 75);
  CHECK(velocity_of(v, 69) == 75);
  check_clean(r);
}

TEST_CASE("Chord presets: voicings", "[presets][chord]")
{
  check_chord("Major seventh first inversion", 60, {64, 67, 71, 72});
  check_chord("Minor triad second inversion", 60, {67, 72, 75});
  check_chord("Seventh with the seventh in the bass", 60, {58, 60, 64, 67});
}

TEST_CASE("Chord presets: harmonizer", "[presets][chord]")
{
  check_chord("Harmonizer major third above", 60, {60, 64});
  check_chord("Harmonizer major third above", 69, {69, 73});
  check_chord("Harmonizer thirds and fifth", 62, {62, 66, 69});
  check_chord("Octave doubling", 60, {60, 72});
  check_chord("Octaves above and below", 60, {48, 60, 72});
  // Out of range voices are left out, the others play
  check_chord("Octaves above and below", 120, {108, 120});
}

TEST_CASE("Chord presets: guitar strum up", "[presets][chord]")
{
  Rig r{"Guitar strum up"};
  auto v = r.strike(52, 100);
  REQUIRE(v.size() == 6);
  CHECK(pitches(v) == std::set<int>{52, 59, 64, 68, 71, 76});
  // Low to high, slowing down (tension above 0), fading (crescendo below 0)
  for(std::size_t i = 1; i < v.size(); i++)
  {
    CHECK(v[i].pitch > v[i - 1].pitch);
    CHECK(v[i].at > v[i - 1].at);
    CHECK(v[i].velocity <= v[i - 1].velocity);
  }
  CHECK(v[0].at == 0);
  CHECK(v[5].at - v[4].at > v[1].at - v[0].at);
  // 12 ms between strings on average
  CHECK(v[5].at == int64_t(0.012 * rate * 5));
  CHECK(v[0].velocity == 100);
  CHECK(v[5].velocity == 80);
  check_clean(r);
}

TEST_CASE("Chord presets: guitar strum alternating", "[presets][chord]")
{
  Rig r{"Guitar strum alternating"};
  auto first = r.strike(52);
  auto second = r.strike(52);
  REQUIRE(first.size() == 6);
  REQUIRE(second.size() == 6);
  // Up, then down
  CHECK(first.front().pitch == 52);
  CHECK(first.back().pitch == 76);
  CHECK(second.front().pitch == 76);
  CHECK(second.back().pitch == 52);
  check_clean(r);
}

TEST_CASE("Chord presets: harp glissando", "[presets][chord]")
{
  Rig r{"Harp glissando"};
  auto v = r.strike(48, 80);
  REQUIRE(v.size() == 9);
  CHECK(pitches(v) == std::set<int>{48, 52, 55, 60, 64, 67, 72, 76, 79});
  for(std::size_t i = 1; i < v.size(); i++)
  {
    CHECK(v[i].pitch > v[i - 1].pitch);
    CHECK(v[i].at > v[i - 1].at);
    CHECK(v[i].velocity >= v[i - 1].velocity);
  }
  // Speeds up: tension below 0
  CHECK(v[8].at - v[7].at < v[1].at - v[0].at);
  CHECK(v[8].at == int64_t(0.05 * rate * 8));
  CHECK(v[8].velocity == 104);
  check_clean(r);
}

TEST_CASE("Chord presets: a key released during the glissando", "[presets][chord]")
{
  Rig r{"Harp glissando"};
  r.tick({libremidi::channel_events::note_on(1, 48, 100)});
  r.run(4);
  r.tick({libremidi::channel_events::note_off(1, 48, 0)});
  r.run(int(rate / frames));
  CHECK(r.ons.size() < 9);
  check_clean(r);
}

TEST_CASE("Chord presets: humanized chord", "[presets][chord]")
{
  Rig r{"Humanized chord"};
  std::map<int, int> count;
  for(int i = 0; i < 60; i++)
  {
    auto v = r.strike(60, 100);
    REQUIRE(!v.empty());
    CHECK(v.front().pitch == 60);
    CHECK(v.front().velocity == 100);
    for(auto& p : v)
    {
      count[p.pitch]++;
      CHECK((p.pitch == 60 || p.pitch == 64 || p.pitch == 67 || p.pitch == 71));
    }
    CHECK(velocity_of(v, 64) <= 85);
    for(std::size_t k = 1; k < v.size(); k++)
      CHECK(v[k].at > v[k - 1].at);
  }
  CHECK(count[60] == 60);
  // 90, 80, 70 percent: some drop, most play
  for(int p : {64, 67, 71})
  {
    INFO(p);
    CHECK(count[p] > 20);
    CHECK(count[p] < 60);
  }
  check_clean(r);
}

TEST_CASE("Chord presets: sparse chord with a fixed seed repeats", "[presets][chord]")
{
  auto take = [] {
    Rig r{"Sparse chord, fixed seed"};
    std::vector<std::set<int>> res;
    for(int i = 0; i < 20; i++)
      res.push_back(pitches(r.strike(57)));
    check_clean(r);
    return res;
  };
  const auto a = take();
  const auto b = take();
  CHECK(a == b);
  int full = 0, partial = 0;
  for(auto& s : a)
  {
    CHECK(s.count(57) == 1);
    for(int p : s)
      CHECK((p == 57 || p == 60 || p == 64 || p == 67));
    (s.size() == 4 ? full : partial)++;
  }
  CHECK(partial > 0);
}

TEST_CASE("Chord presets: diatonic chords on white keys", "[presets][chord]")
{
  Rig r{"Diatonic chords on white keys"};
  CHECK(pitches(r.strike(60)) == std::set<int>{60, 64, 67});
  CHECK(pitches(r.strike(62)) == std::set<int>{62, 65, 69});
  CHECK(pitches(r.strike(64)) == std::set<int>{64, 67, 71});
  CHECK(pitches(r.strike(65)) == std::set<int>{65, 69, 72});
  CHECK(pitches(r.strike(67)) == std::set<int>{67, 71, 74, 77});
  CHECK(pitches(r.strike(69)) == std::set<int>{69, 72, 76});
  CHECK(pitches(r.strike(71)) == std::set<int>{71, 74, 77});
  // A key without a chord plays itself
  CHECK(pitches(r.strike(61)) == std::set<int>{61});
  check_clean(r);
}

TEST_CASE("Chord presets: MPE chord", "[presets][chord]")
{
  Rig r{"MPE chord"};
  r.tick({libremidi::channel_events::note_on(1, 60, 100)});
  REQUIRE(r.ons.size() == 4);
  std::set<int> channels;
  for(auto& p : r.ons)
  {
    channels.insert(p.channel);
    CHECK(p.channel >= 2);
    CHECK(p.channel <= 16);
  }
  CHECK(channels.size() == 4);
  CHECK(pitches(r.ons) == std::set<int>{60, 64, 67, 71});

  // The bend of the key goes to every voice of its chord
  r.others.clear();
  r.tick({libremidi::channel_events::pitch_bend(1, 10000)});
  std::set<int> bent;
  for(auto& m : r.others)
    if(m.get_message_type() == libremidi::message_type::PITCH_BEND)
      bent.insert(m.get_channel());
  CHECK(bent == channels);

  r.tick({libremidi::channel_events::note_off(1, 60, 0)});
  r.run(2);
  check_clean(r);
}

// The strum is a time chooser: synced, its value is in whole notes and follows
// the tempo of the tick.
TEST_CASE("Chord presets: strum in sixty-fourth notes", "[presets][chord]")
{
  Rig r{"Strum in sixty-fourth notes"};
  auto v = r.strike(60);
  REQUIRE(v.size() == 4);
  // At 120 BPM a whole note lasts 2 s: 1/64 is 31.25 ms, 1500 frames
  CHECK(v[1].at - v[0].at == 1500);
  CHECK(v[3].at - v[0].at == 4500);
  check_clean(r);
}
