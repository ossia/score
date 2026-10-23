// The Midi random sequencer presets of the default package: each one plays
// what its name says, and every note it starts, it stops.

#include "MidiPresetRig.hpp"

#include <Advanced/MidiScaler/MidiRandomSequencer.hpp>

#include <algorithm>
#include <set>

using namespace midi_presets;

namespace
{
using Seq = mtk::MidiRandomSequencer;
using Rig = rig<Seq>;
const QString folder = "Midi random sequencer";
const QString uuid = "1fbb46dd-c422-40aa-8d0b-b9237ba91aa8";

void setup(Rig& r, const char* name)
{
  const auto errors = r.load(find(folder, uuid, name));
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
}

void finish(Rig& r, int64_t until, const Rig::input& in = {})
{
  r.run(until, in);
  r.stop();
  r.check_clean();
}

std::set<int> pitch_classes(const Rig& r)
{
  std::set<int> s;
  for(int p : r.on_pitches())
    s.insert(p % 12);
  return s;
}

bool subset(const std::set<int>& a, const std::set<int>& b)
{
  return std::includes(b.begin(), b.end(), a.begin(), a.end());
}

int period_of(const std::vector<int>& v, int max_period)
{
  for(int p = 1; p <= max_period; p++)
  {
    bool ok = true;
    for(std::size_t i = p; i < v.size() && ok; i++)
      ok = v[i] == v[i - p];
    if(ok)
      return p;
  }
  return 0;
}

void check_range(const Rig& r, int lo, int hi)
{
  for(int p : r.on_pitches())
  {
    CHECK(p >= lo);
    CHECK(p <= hi);
  }
}

//! Every note-on on the grid of `step` frames.
void check_on_grid(const Rig& r, int64_t step)
{
  for(auto at : r.on_dates())
    CHECK(at % step == 0);
}

const std::set<int> a_minor{9, 11, 0, 2, 4, 5, 7};
const std::set<int> c_major{0, 2, 4, 5, 7, 9, 11};
const std::set<int> d_dorian{2, 4, 5, 7, 9, 11, 0};
}

TEST_CASE("Midi random sequencer presets load", "[presets][midi_random_sequencer]")
{
  const auto presets = load(folder, uuid);
  REQUIRE(presets.size() >= 12);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    Seq s;
    const auto errors = apply_as_host(s.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
  }
}

TEST_CASE("Preset: Locked 16-step bass line", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Locked 16-step bass line");
  finish(r, 8 * bar);
  // The same bar over and over: a seeded locked register, with rests
  std::vector<int> dates;
  for(auto at : r.on_dates())
    dates.push_back(int((at / sixteenth) % 16));
  const auto n = r.ons().size();
  CHECK(n % 8 == 0);
  CHECK(n < 8 * 16);
  CHECK(n > 8 * 4);
  const auto p = r.on_pitches();
  const int per = period_of(p, int(n / 8));
  CHECK(per > 0);
  CHECK((n / 8) % per == 0);
  CHECK(subset(pitch_classes(r), a_minor));
  check_range(r, 33, 52);
  check_on_grid(r, sixteenth);
}

TEST_CASE("Preset: Slowly mutating melody", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Slowly mutating melody");
  finish(r, 32 * bar);
  const auto p = r.on_pitches();
  REQUIRE(p.size() == 32 * 8);
  int same = 0;
  for(std::size_t i = 8; i < p.size(); i++)
    same += p[i] == p[i - 8];
  // Mostly the loop of the previous bar, but never frozen
  CHECK(same > int(p.size()) / 3);
  CHECK(same < int(p.size()) - 16);
  CHECK(subset(pitch_classes(r), c_major));
  check_range(r, 60, 84);
  check_on_grid(r, 2 * sixteenth);
}

TEST_CASE("Preset: Double length inverted loop", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Double length inverted loop");
  finish(r, 16 * bar);
  const auto p = r.on_pitches();
  REQUIRE(p.size() == 16 * 16);
  std::vector<int> tail(p.begin() + 16, p.end());
  CHECK(period_of(tail, 32) == 16);
  CHECK(subset(pitch_classes(r), d_dorian));
}

TEST_CASE("Preset: Acid line with accents", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Acid line with accents");
  finish(r, 16 * bar);
  int accents = 0, soft = 0;
  for(auto& e : r.ons())
  {
    if(e.data2 == 127)
      accents++;
    else if(e.data2 <= 99)
      soft++;
  }
  CHECK(accents > 0);
  CHECK(soft > 0);
  CHECK(accents + soft == int(r.ons().size()));
  CHECK(r.ons().size() < 16 * 16); // rests
  CHECK(subset(pitch_classes(r), a_minor));
  check_range(r, 36, 72);
  check_on_grid(r, sixteenth);
  // Short notes, no ratchets: one note per step at most
  const auto all = r.on_dates();
  std::set<int64_t> dates(all.begin(), all.end());
  CHECK(dates.size() == r.ons().size());
}

TEST_CASE("Preset: Fully random arpeggio", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Fully random arpeggio");
  finish(r, 8 * bar);
  CHECK(r.ons().size() == 8 * 16);
  CHECK(pitch_classes(r) == std::set<int>{0, 4, 7});
  CHECK(period_of(r.on_pitches(), 32) == 0);
  check_range(r, 60, 84);
}

TEST_CASE("Preset: Pentatonic wind chimes", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Pentatonic wind chimes");
  finish(r, 32 * bar);
  const auto n = int(r.ons().size());
  CHECK(n > 32 * 8 / 8);
  CHECK(n < 32 * 8 / 2);
  CHECK(subset(pitch_classes(r), {0, 2, 4, 7, 9}));
  check_range(r, 72, 96);
  // Off the grid, but never before the step
  int off_grid = 0;
  for(auto at : r.on_dates())
  {
    const int64_t d = at % (2 * sixteenth);
    CHECK(d <= sixteenth);
    off_grid += d != 0;
  }
  CHECK(off_grid > n / 2);
}

TEST_CASE("Preset: Ambient dorian drift", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Ambient dorian drift");
  finish(r, 32 * bar);
  const auto n = int(r.ons().size());
  CHECK(n > 32 * 4 / 3);
  CHECK(n < 32 * 4);
  CHECK(subset(pitch_classes(r), d_dorian));
  check_range(r, 50, 74);
  for(auto& e : r.ons())
  {
    CHECK(e.data2 >= 40);
    CHECK(e.data2 <= 70);
  }
  // Long notes: on average longer than a quarter
  int64_t total = 0;
  for(auto& e : r.ons())
  {
    const auto end = r.end_of(e.data1, e.at);
    REQUIRE(end > e.at);
    total += end - e.at;
  }
  CHECK(total / n > quarter / 2);
}

TEST_CASE("Preset: Coarse steps root and fifth bass", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Coarse steps root and fifth bass");
  finish(r, 16 * bar);
  CHECK(pitch_classes(r) == std::set<int>{4, 11});
  check_range(r, 28, 52);
  check_on_grid(r, 2 * sixteenth);
}

TEST_CASE("Preset: Marbles random melody", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Marbles random melody");
  finish(r, 16 * bar);
  CHECK(subset(pitch_classes(r), {7, 9, 11, 0, 2, 4, 5}));
  CHECK(pitch_classes(r).size() >= 5);
  check_range(r, 55, 79);
  CHECK(r.ons().size() > 16 * 16 / 2);
  CHECK(r.ons().size() < 16 * 16);
}

TEST_CASE("Preset: Sparse percussion", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Sparse percussion");
  finish(r, 16 * bar);
  const auto n = int(r.ons().size());
  CHECK(n > 16 * 16 / 5);
  CHECK(n < 16 * 16 * 3 / 5);
  for(auto& e : r.events)
    CHECK(e.channel == 10);
  check_range(r, 36, 51);
  CHECK(pitch_classes(r).size() > 6);
  check_on_grid(r, sixteenth);
}

TEST_CASE("Preset: Tresillo loop", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Tresillo loop");
  finish(r, 8 * bar);
  // 3 + 3 + 2 sixteenths, every half bar, the same three notes
  std::vector<int64_t> want;
  for(int k = 0; k < 16; k++)
    for(int s : {0, 3, 6})
      want.push_back((k * 8 + s) * sixteenth);
  CHECK(r.on_dates() == want);
  CHECK(period_of(r.on_pitches(), 3) == 3);
  CHECK(subset(pitch_classes(r), {9, 0, 2, 4, 7}));
}

TEST_CASE("Preset: Swung pulses", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Swung pulses");
  finish(r, 16 * bar);
  const auto n = int(r.ons().size());
  CHECK(n > 16 * 16 / 4);
  CHECK(n < 16 * 16 * 3 / 4);
  int late = 0;
  for(auto at : r.on_dates())
  {
    const int64_t d = at % (2 * sixteenth);
    CHECK((d == 0 || d == sixteenth + sixteenth / 3));
    late += d != 0;
  }
  CHECK(late > 0);
}

TEST_CASE("Preset: Clocked by input notes", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Clocked by input notes");
  Rig::input in;
  for(int i = 0; i < 12; i++)
  {
    in.push_back({i * 10000 + 777, on(40 + i)});
    in.push_back({i * 10000 + 4000, off(40 + i)});
  }
  finish(r, 2 * bar, in);
  CHECK(r.ons().size() == 12);
  for(int i = 0; i < 12; i++)
    CHECK(r.on_dates()[i] == i * 10000 + 777);
  // The played notes are consumed: only the sequence comes out
  for(int p : r.on_pitches())
    CHECK(p >= 57);
  CHECK(subset(pitch_classes(r), {9, 0, 2, 4, 7}));
}

TEST_CASE("Preset: Free running 200 ms", "[presets][midi_random_sequencer]")
{
  Rig r;
  setup(r, "Free running 200 ms");
  finish(r, 4 * bar);
  // 9600 frames apart whatever the tempo
  const auto d = r.on_dates();
  REQUIRE(d.size() == 40);
  for(std::size_t i = 0; i < d.size(); i++)
    CHECK(d[i] == int64_t(i) * 9600);
}
