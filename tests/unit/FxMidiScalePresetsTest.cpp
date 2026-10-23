// The Midi scale presets of the default package: each one loads onto the
// object and moves the notes where its name says, with every note-on paired
// with one note-off.

#include "MidiPresetHarness.hpp"

#include <Fx/MidiUtil.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Scale = Nodes::MidiUtil::Node;

const std::vector<midi_presets::preset>& presets()
{
  static const auto p
      = midi_presets::load("Midi scale", "06b33b83-bb67-4f7a-9980-f5d66e4266c5");
  return p;
}

struct Rig
{
  Scale node;
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;

  explicit Rig(const char* name)
  {
    for(auto& p : presets())
    {
      if(p.name == name)
      {
        for(auto& e : midi_presets::apply(node.inputs, p))
          errors.push_back(e.toStdString());
        return;
      }
    }
    errors.push_back(std::string("missing preset ") + name);
  }

  std::vector<libremidi::message> tick(std::vector<libremidi::message> in)
  {
    node.inputs.midi.midi_messages.assign(in.begin(), in.end());
    node.outputs.midi.midi_messages.clear();
    node(halp::tick_flicks{});
    std::vector<libremidi::message> out{
        node.outputs.midi.midi_messages.begin(), node.outputs.midi.midi_messages.end()};
    for(auto& m : out)
    {
      const auto t = m.get_message_type();
      if(t != libremidi::message_type::NOTE_ON && t != libremidi::message_type::NOTE_OFF)
        continue;
      auto& n = sounding[{m.get_channel(), m.bytes[1]}];
      if(t == libremidi::message_type::NOTE_ON && m.bytes[2] > 0)
      {
        if(n++ > 0)
          errors.push_back("started twice " + std::to_string(m.bytes[1]));
      }
      else if(n == 0)
        errors.push_back("stopped while silent " + std::to_string(m.bytes[1]));
      else
        n--;
    }
    return out;
  }

  //! The note a key plays (-1: none), after pressing and releasing it.
  int play(int pitch, int ch = 1)
  {
    auto m = libremidi::channel_events::note_on(ch, pitch, 100);
    m.timestamp = 17;
    auto out = tick({m});
    int res = -1;
    for(auto& o : out)
    {
      if(o.get_message_type() == libremidi::message_type::NOTE_ON)
      {
        res = o.bytes[1];
        if(o.timestamp != 17)
          errors.push_back("timestamp moved");
        if(o.get_channel() != ch)
          errors.push_back("channel changed");
        if(o.bytes[2] != 100)
          errors.push_back("velocity changed");
      }
    }
    tick({libremidi::channel_events::note_off(ch, pitch, 0)});
    return res;
  }

  int still_sounding() const
  {
    int total = 0;
    for(auto& [k, n] : sounding)
      total += n;
    return total;
  }
};

//! Each key from 48 to 71 and the note it must play.
void check_map(const char* name, const std::map<int, int>& expected)
{
  INFO(name);
  Rig r{name};
  for(auto [in, out] : expected)
  {
    INFO("key " << in);
    CHECK(r.play(in) == out);
  }
  // A chord held on two channels, then released
  r.tick(
      {libremidi::channel_events::note_on(1, 60, 90),
       libremidi::channel_events::note_on(1, 61, 90),
       libremidi::channel_events::note_on(2, 62, 90)});
  r.tick(
      {libremidi::channel_events::note_off(1, 60, 0),
       libremidi::channel_events::note_off(1, 61, 0),
       libremidi::channel_events::note_off(2, 62, 0)});
  INFO((r.errors.empty() ? std::string{} : r.errors.front()));
  CHECK(r.errors.empty());
  CHECK(r.still_sounding() == 0);
}

//! White keys of one octave, and the black keys to the note above.
std::map<int, int> white_keys_up(int octave)
{
  const int c = 12 * octave;
  return {{c, c},           {c + 1, c + 2},  {c + 2, c + 2}, {c + 3, c + 4},
          {c + 4, c + 4},   {c + 5, c + 5},  {c + 6, c + 7}, {c + 7, c + 7},
          {c + 8, c + 9},   {c + 9, c + 9},  {c + 10, c + 11}, {c + 11, c + 11}};
}
}

TEST_CASE("Midi scale presets: every preset loads", "[presets][midi_scale]")
{
  REQUIRE(presets().size() >= 6);
  for(auto& p : presets())
  {
    INFO(p.name.toStdString());
    Scale s;
    const auto errors = midi_presets::apply(s.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
  }
}

TEST_CASE("Midi scale presets: diatonic scales", "[presets][midi_scale]")
{
  check_map("C major", white_keys_up(5));
  // Relative modes of C major: the same white keys
  check_map("A minor", white_keys_up(5));
  check_map("D dorian", white_keys_up(4));
}

TEST_CASE("Midi scale presets: custom masks", "[presets][midi_scale]")
{
  // C D E G A; out of scale notes go to the nearest, above on a tie
  check_map(
      "Pentatonic on C",
      {{60, 60}, {61, 62}, {62, 62}, {63, 64}, {65, 64}, {66, 67},
       {68, 69}, {70, 69}, {71, 72}, {72, 72}});
  // C Eb F F# G Bb
  check_map(
      "Blues scale on C",
      {{60, 60}, {61, 60}, {62, 63}, {64, 65}, {66, 66}, {68, 67}, {69, 70}, {71, 72}});
}

TEST_CASE("Midi scale presets: triad notes", "[presets][midi_scale]")
{
  check_map(
      "C major triad notes",
      {{60, 60}, {61, 60}, {62, 64}, {63, 64}, {65, 64}, {66, 67}, {69, 67}, {70, 72},
       {71, 72}});
}

TEST_CASE("Midi scale presets: directions", "[presets][midi_scale]")
{
  check_map("C major, up only", {{60, 60}, {61, 62}, {63, 64}, {66, 67}, {68, 69}, {70, 71}});
  check_map(
      "C major, down only", {{60, 60}, {61, 60}, {63, 62}, {66, 65}, {68, 67}, {70, 69}});
}

TEST_CASE("Midi scale presets: transposition", "[presets][midi_scale]")
{
  check_map("Octave up", {{60, 72}, {61, 73}, {36, 48}, {127, 127}});
  check_map("Fifth down", {{60, 53}, {67, 60}, {3, 0}});
}

TEST_CASE("Midi scale presets: remaps", "[presets][midi_scale]")
{
  check_map(
      "Major to minor",
      {{60, 60}, {62, 62}, {64, 63}, {65, 65}, {67, 67}, {69, 68}, {71, 70}, {76, 75}});
  check_map(
      "Major to harmonic minor",
      {{60, 60}, {62, 62}, {64, 63}, {65, 65}, {67, 67}, {69, 68}, {71, 71}});
  check_map("Major to mixolydian", {{60, 60}, {64, 64}, {71, 70}, {59, 58}});
  check_map("Major to lydian", {{60, 60}, {65, 66}, {77, 78}, {67, 67}});
  check_map(
      "Black keys to the white key above",
      {{60, 60}, {61, 62}, {63, 64}, {66, 67}, {68, 69}, {70, 71}, {71, 71}});
}

TEST_CASE("Midi scale presets: matrix", "[presets][midi_scale]")
{
  // Ableton's inversion: C to the C above, then down the diagonal
  check_map(
      "Invert melody around C",
      {{60, 72}, {62, 70}, {64, 68}, {65, 67}, {67, 65}, {69, 63}, {71, 61}});
  // Mirrored around C, each note moving by 6 semitones at most
  check_map(
      "Invert melody, same register",
      {{60, 60}, {62, 58}, {64, 68}, {65, 67}, {67, 65}, {69, 63}, {71, 73}});
  check_map(
      "Mute the black keys",
      {{60, 60}, {61, -1}, {62, 62}, {63, -1}, {66, -1}, {68, -1}, {70, -1}, {71, 71}});
  check_map("Fifth up above C4 only", {{59, 59}, {48, 48}, {60, 67}, {72, 79}, {120, 127}});
}

// "custom" is one of the entries of the Scale list: picking it in the UI must
// not silence the object.
TEST_CASE("Midi scale: custom picked from the list", "[presets][midi_scale]")
{
  Scale s;
  s.inputs.sc.value = "custom";
  s.inputs.midi.midi_messages.push_back(libremidi::channel_events::note_on(1, 60, 100));
  s(halp::tick_flicks{});
  CHECK(s.outputs.midi.midi_messages.size() == 1);
}
