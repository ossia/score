// The Midi progression player presets of the default package: each one plays
// the progression its name says, in its key, at its pace, and every note it
// starts, it stops.

#include "MidiPresetRig.hpp"

#include <Advanced/MidiScaler/MidiProgressionPlayer.hpp>

#include <set>
#include <sstream>

using namespace midi_presets;

namespace
{
using Player = mtk::MidiProgressionPlayer;
using Rig = rig<Player>;
const QString folder = "Midi progression player";
const QString uuid = "213d75e7-3c2e-4b13-9686-fbfd2132fc2d";

libremidi::message note_on(int p, int ch = 1)
{
  return libremidi::channel_events::note_on(ch, p, 100);
}
libremidi::message note_off(int p, int ch = 1)
{
  return libremidi::channel_events::note_off(ch, p, 0);
}

void setup(Rig& r, const char* name)
{
  const auto errors = r.load(find(folder, uuid, name));
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
}

int pc_of(const std::string& n)
{
  static const std::string names = "C D EF G A B";
  int pc = int(names.find(n[0]));
  for(std::size_t i = 1; i < n.size(); i++)
    pc += n[i] == '#' ? 1 : (n[i] == 'b' ? -1 : 0);
  return ((pc % 12) + 12) % 12;
}

std::set<int> pcs(const std::string& notes)
{
  std::set<int> r;
  std::istringstream s{notes};
  std::string n;
  while(s >> n)
    r.insert(pc_of(n));
  return r;
}

struct chord_at
{
  int64_t at;
  std::set<int> pcs;
  int lowest;
};

//! Note-ons on `channel` grouped by date.
std::vector<chord_at> chords(const Rig& r, int channel = 1)
{
  std::vector<chord_at> res;
  for(const auto& e : r.events)
    if(e.on() && e.channel == channel)
    {
      if(res.empty() || res.back().at != e.at)
        res.push_back({e.at, {}, 128});
      res.back().pcs.insert(e.data1 % 12);
      res.back().lowest = std::min(res.back().lowest, e.data1);
    }
  return res;
}

void check_chords(
    const Rig& r, int64_t step, const std::vector<std::string>& want, int channel = 1)
{
  const auto c = chords(r, channel);
  REQUIRE(c.size() >= want.size());
  for(std::size_t i = 0; i < want.size(); i++)
  {
    INFO("chord " << i << " " << want[i]);
    if(step > 0)
      CHECK(c[i].at == int64_t(i) * step);
    CHECK(c[i].pcs == pcs(want[i]));
  }
}

void finish(Rig& r)
{
  r.stop();
  r.check_clean();
}
}

TEST_CASE("Midi progression player presets load", "[presets][midi_progression_player]")
{
  const auto presets = load(folder, uuid);
  REQUIRE(presets.size() >= 12);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    Player d;
    const auto errors = apply_as_host(d.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
    // The custom text of every preset parses
    mtk::parsed_progression parsed;
    CHECK(!mtk::parse_progression(d.inputs.custom.value, parsed));
  }
}

TEST_CASE("Preset: Pop I V vi IV", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Pop I V vi IV");
  r.run(4 * bar);
  check_chords(r, bar, {"C E G", "G B D", "A C E", "F A C"});
  // The bass under each chord is its root, at most an octave below C4
  const auto c = chords(r);
  for(int i = 0; i < 4; i++)
    CHECK(c[i].lowest <= 48);
  CHECK(c[0].lowest % 12 == 0);
  CHECK(c[1].lowest % 12 == 7);
  finish(r);
}

TEST_CASE("Preset: Fifties I vi IV V", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Fifties I vi IV V");
  r.run(2 * bar);
  check_chords(r, bar / 2, {"C E G", "A C E", "F A C", "G B D"});
  finish(r);
}

TEST_CASE("Preset: Royal road IV V iii vi", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Royal road IV V iii vi");
  r.run(4 * bar);
  // In D: Gmaj7 A7 F#m7 Bm
  check_chords(r, bar, {"G B D F#", "A C# E G", "F# A C# E", "B D F#"});
  CHECK(r.node.outputs.chord.value == "Bm");
  finish(r);
}

TEST_CASE("Preset: Quarter note stabs", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Quarter note stabs");
  r.run(bar);
  check_chords(r, quarter, {"C E G", "E G B", "A C E", "F A C"});
  // Short: about a third of the beat
  const auto c = chords(r);
  const auto ons = r.ons();
  CHECK(r.end_of(ons[0].data1, 0) == quarter * 35 / 100);
  finish(r);
}

TEST_CASE("Preset: Jazz ii V I sevenths", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Jazz ii V I sevenths");
  r.run(4 * bar);
  check_chords(r, bar, {"C Eb G Bb", "F A C Eb", "Bb D F A", "Bb D F A"});
  finish(r);
}

TEST_CASE("Preset: Blues 12 bar", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Blues 12 bar");
  r.run(13 * bar);
  const std::string A7 = "A C# E G", D7 = "D F# A C", E7 = "E G# B D";
  check_chords(r, bar, {A7, D7, A7, A7, D7, D7, A7, A7, E7, D7, A7, E7, A7});
  finish(r);
}

TEST_CASE("Preset: Sad minor i VI III VII", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Sad minor i VI III VII");
  r.run(4 * bar);
  check_chords(r, bar, {"A C E", "F A C", "C E G", "G B D"});
  CHECK(r.node.outputs.chord.value == "G");
  finish(r);
}

TEST_CASE("Preset: Andalusian cadence", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Andalusian cadence");
  r.run(4 * bar);
  check_chords(r, bar, {"A C E", "G B D", "F A C", "E G# B"});
  finish(r);
}

TEST_CASE("Preset: Dorian vamp", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Dorian vamp");
  r.run(8 * bar);
  // D dorian: Dm7 and G7, two bars each
  check_chords(r, 2 * bar, {"D F A C", "G B D F", "D F A C", "G B D F"});
  finish(r);
}

TEST_CASE("Preset: Minor to major cadence on trigger", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Minor to major cadence on trigger");
  r.run(bar);
  CHECK(r.ons().empty());
  for(int k = 0; k < 4; k++)
  {
    r.node.inputs.next.value.emplace();
    r.run(r.pos + quarter);
  }
  check_chords(r, 0, {"F Ab C", "Eb G Bb", "Ab C Eb", "C E G"});
  finish(r);
}

TEST_CASE("Preset: Keys play degrees", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Keys play degrees");
  // White keys from C4 over vi (Am): A C E, then A an octave up
  Rig::input in;
  int64_t t = 1000;
  for(int p : {60, 62, 64, 65})
  {
    in.push_back({t, note_on(p, 2)});
    in.push_back({t + 500, note_off(p, 2)});
    t += 1000;
  }
  r.run(bar, in);
  std::vector<int> played;
  for(const auto& e : r.events)
    if(e.on() && e.channel == 2)
      played.push_back(e.data1);
  CHECK(played == std::vector<int>{57, 60, 64, 69});
  // The chords play too, low
  check_chords(r, 0, {"A C E"});
  finish(r);
}

TEST_CASE("Preset: Snap keyboard to chords", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "Snap keyboard to chords");
  // C# over C major goes to C; F# over G major to G
  r.run(
      2 * bar, {{1000, note_on(61, 3)},
                {2000, note_off(61, 3)},
                {bar + 1000, note_on(66, 3)},
                {bar + 2000, note_off(66, 3)}});
  std::vector<int> played;
  for(const auto& e : r.events)
    if(e.on() && e.channel == 3)
      played.push_back(e.data1);
  CHECK(played == std::vector<int>{60, 67});
  finish(r);
}

TEST_CASE("Preset: One chord per key press", "[presets][midi_progression_player]")
{
  Rig r;
  setup(r, "One chord per key press");
  r.run(
      bar, {{1000, note_on(36, 5)},
            {9000, note_off(36, 5)},
            {12000, note_on(38, 5)},
            {20000, note_off(38, 5)},
            {30000, note_on(36, 5)},
            {40000, note_off(36, 5)}});
  const auto c = chords(r);
  REQUIRE(c.size() == 3);
  CHECK(c[0].at == 1000);
  CHECK(c[1].at == 12000);
  CHECK(c[2].at == 30000);
  CHECK(c[0].pcs == pcs("D F A"));
  CHECK(c[1].pcs == pcs("G B D"));
  CHECK(c[2].pcs == pcs("C E G"));
  // The keys themselves are consumed
  for(const auto& e : r.events)
    CHECK(e.channel != 5);
  finish(r);
}
