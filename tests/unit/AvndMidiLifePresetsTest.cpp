// The Midi life presets of the default package: each one plays what its name
// says, and every note it starts, it stops.

#include "MidiPresetRig.hpp"

#include <Advanced/MidiScaler/MidiLife.hpp>

#include <algorithm>
#include <map>
#include <set>

using namespace midi_presets;

namespace
{
using Life = mtk::MidiLife;
using Rig = rig<Life>;
const QString folder = "Midi life";
const QString uuid = "04db97fd-2ccf-4271-a5a6-d66552baf0d7";

void setup(Rig& r, const char* name)
{
  const auto errors = r.load(find(folder, uuid, name));
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
}

void finish(Rig& r)
{
  r.stop();
  r.check_clean();
}

//! Every note is in the scale: semitones above the root's pitch class.
void check_scale(const Rig& r, int root, std::set<int> steps)
{
  for(auto& e : r.ons())
  {
    INFO("note " << e.data1 << " at " << e.at);
    CHECK(steps.count(((e.data1 - root) % 12 + 12) % 12) == 1);
  }
}

//! Every note starts on the grid of `step` frames, or `late` frames after.
void check_grid(const Rig& r, int64_t step, int64_t late = 0)
{
  for(auto& e : r.ons())
  {
    INFO("at " << e.at);
    const auto off = e.at % step;
    CHECK((off == 0 || off == late));
  }
}

//! The most notes starting on one frame.
int max_chord(const Rig& r)
{
  std::map<int64_t, int> n;
  for(auto& e : r.ons())
    n[e.at]++;
  int m = 0;
  for(auto& [k, v] : n)
    m = std::max(m, v);
  return m;
}

const std::set<int> major_penta{0, 2, 4, 7, 9};
const std::set<int> minor_penta{0, 3, 5, 7, 10};
const std::set<int> dorian{0, 2, 3, 5, 7, 9, 10};
const std::set<int> minor{0, 2, 3, 5, 7, 8, 10};
const std::set<int> lydian{0, 2, 4, 6, 7, 9, 11};
const std::set<int> mixolydian{0, 2, 4, 5, 7, 9, 10};
const std::set<int> harmonic_minor{0, 2, 3, 5, 7, 8, 11};
}

TEST_CASE("Midi life presets load", "[presets][midi_life]")
{
  const auto presets = load(folder, uuid);
  REQUIRE(presets.size() >= 12);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    Life l;
    const auto errors = apply_as_host(l.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
  }
}

TEST_CASE("Midi life presets all play and end their notes", "[presets][midi_life]")
{
  for(const auto& p : load(folder, uuid))
  {
    INFO(p.name.toStdString());
    Rig r;
    REQUIRE(r.load(p).isEmpty());
    r.run(8 * bar);
    CHECK(r.ons().size() > 4);
    finish(r);
  }
}

TEST_CASE("Midi life preset: Glider melody pentatonic", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Glider melody pentatonic");
  for(int i = 0; i < 64; i++)
  {
    r.run(r.pos + bar / 2);
    CHECK(r.node.population() == 5); // the glider glides on, whole
  }
  check_scale(r, 0, major_penta);
  check_grid(r, quarter / 2);
  CHECK(max_chord(r) <= 2);
  for(auto& e : r.ons())
    CHECK(e.data1 >= 60);
  // The glider moves: the tune changes
  std::set<int> pitches;
  for(auto& e : r.ons())
    pitches.insert(e.data1);
  CHECK(pitches.size() >= 5);
  finish(r);
}

TEST_CASE("Midi life preset: R-pentomino chaos in dorian", "[presets][midi_life]")
{
  Rig r;
  setup(r, "R-pentomino chaos in dorian");
  int most = 0;
  for(int i = 0; i < 16; i++)
  {
    r.run(r.pos + bar);
    most = std::max(most, r.node.population());
  }
  CHECK(most > 12); // five cells that grow into a crowd
  check_scale(r, 2, dorian);
  CHECK(max_chord(r) <= 3);
  finish(r);
}

TEST_CASE("Midi life preset: HighLife minor", "[presets][midi_life]")
{
  Rig r;
  setup(r, "HighLife minor");
  r.run(16 * bar);
  check_scale(r, 9, minor);
  check_grid(r, sixteenth);
  CHECK(max_chord(r) <= 3);
  CHECK(r.node.inputs.rule.value == Life::RuleHighLife);
  finish(r);
}

TEST_CASE("Midi life preset: Births only bells", "[presets][midi_life]")
{
  Rig births;
  setup(births, "Births only bells");
  births.run(16 * bar);
  Rig all;
  setup(all, "Births only bells");
  all.node.inputs.play.value = Life::PlayLive;
  all.run(16 * bar);
  // The same world: births are a part of the live cells
  CHECK(births.ons().size() > 8);
  CHECK(births.ons().size() < all.ons().size());
  check_scale(births, 5, lydian);
  for(auto& e : births.ons())
    CHECK(e.data1 >= 77); // F5 and up: bells
  finish(births);
  finish(all);
}

TEST_CASE("Midi life preset: Spaceship bassline", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Spaceship bassline");
  r.run(16 * bar);
  CHECK(max_chord(r) == 1);
  check_scale(r, 4, minor_penta);
  for(auto& e : r.ons())
    CHECK(e.data1 < 48);
  finish(r);
}

TEST_CASE("Midi life preset: Blinker pulse", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Blinker pulse");
  r.run(8 * bar);
  // Three blinkers of period 2, eight generations a bar: every bar plays
  // the same
  std::vector<std::vector<int>> bars(8);
  for(auto& e : r.ons())
    bars[e.at / bar].push_back(int(e.at % bar) * 1000 + e.data1);
  for(std::size_t i = 1; i < bars.size(); i++)
    CHECK(bars[i] == bars[0]);
  CHECK(bars[0].size() >= 8);
  check_scale(r, 0, minor_penta);
  finish(r);
}

TEST_CASE("Midi life preset: Seeds percussion", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Seeds percussion");
  r.run(16 * bar);
  const std::set<int> kit{36, 38, 42, 46, 39, 49};
  std::set<int> used;
  for(auto& e : r.ons())
  {
    CHECK(e.channel == 10);
    CHECK(kit.count(e.data1) == 1);
    used.insert(e.data1);
  }
  CHECK(used.size() >= 4);
  CHECK(max_chord(r) <= 2);
  finish(r);
}

TEST_CASE("Midi life preset: Maze ostinato", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Maze ostinato");
  r.run(16 * bar);
  check_scale(r, 7, mixolydian);
  // Swing 40: the off-beat sixteenths come late
  const int64_t late = std::llround(0.4 * 2. / 3. * sixteenth);
  check_grid(r, 2 * sixteenth, sixteenth + late);
  bool swung = false;
  for(auto& e : r.ons())
    swung |= e.at % (2 * sixteenth) == sixteenth + late;
  CHECK(swung);
  finish(r);
}

TEST_CASE("Midi life preset: Sparse ambient", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Sparse ambient");
  r.run(32 * bar);
  check_grid(r, quarter);
  check_scale(r, 0, lydian);
  CHECK(max_chord(r) <= 2);
  CHECK(r.ons().size() < 32 * 6); // a few notes a bar
  // Gate 100: a note lasts a quarter, unless the next step plays it again
  for(auto& e : r.ons())
  {
    const auto end = r.end_of(e.data1, e.at);
    CHECK((end == e.at + quarter || end == -1));
  }
  finish(r);
}

TEST_CASE("Midi life preset: Day and Night drones", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Day and Night drones");
  r.run(32 * bar);
  check_grid(r, 2 * quarter);
  check_scale(r, 0, minor_penta);
  CHECK(max_chord(r) == 3);
  for(auto& e : r.ons())
    CHECK(e.data1 < 60);
  finish(r);
}

TEST_CASE("Midi life preset: Pentadecathlon 15 sweep cycle", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Pentadecathlon 15 sweep cycle");
  const int64_t sweep = 20 * sixteenth;
  r.run(30 * sweep);
  std::vector<std::vector<int>> sweeps(30);
  for(auto& e : r.ons())
    sweeps[e.at / sweep].push_back(int(e.at % sweep) * 1000 + e.data1);
  for(int i = 0; i < 15; i++)
  {
    INFO("sweep " << i);
    CHECK(sweeps[i] == sweeps[i + 15]);
  }
  CHECK(sweeps[0] != sweeps[1]);
  check_scale(r, 0, major_penta);
  finish(r);
}

TEST_CASE("Midi life preset: Pulsar triplets", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Pulsar triplets");
  std::set<int> pops;
  for(int i = 0; i < 48; i++)
  {
    r.run(r.pos + 6 * (quarter / 3));
    pops.insert(r.node.population());
  }
  CHECK(pops == std::set<int>{48, 56, 72}); // the pulsar's three phases
  check_grid(r, quarter / 3);
  CHECK(max_chord(r) <= 3);
  finish(r);
}

TEST_CASE("Midi life preset: Acorn bloom", "[presets][midi_life]")
{
  Rig r;
  setup(r, "Acorn bloom");
  int most = 0;
  for(int i = 0; i < 16; i++)
  {
    r.run(r.pos + bar);
    most = std::max(most, r.node.population());
  }
  CHECK(most > 30); // seven cells bloom
  check_scale(r, 4, harmonic_minor);
  finish(r);
}
