// The Midi sustain presets of the default package: each one loads onto the
// object and holds the notes its name says, with every note-on paired with one
// note-off.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiSustain.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Sustain = mtk::MidiSustain;
constexpr int frames = 64;

const std::vector<midi_presets::preset>& presets()
{
  static const auto p
      = midi_presets::load("Midi sustain", "943f891c-e56f-4e60-a270-3420657fe368");
  return p;
}

libremidi::message on(int p, int ch = 1)
{
  auto m = libremidi::channel_events::note_on(ch, p, 100);
  m.timestamp = 5;
  return m;
}
libremidi::message off(int p, int ch = 1)
{
  auto m = libremidi::channel_events::note_off(ch, p, 0);
  m.timestamp = 9;
  return m;
}
libremidi::message pedal(bool down, int ch = 1)
{
  auto m = libremidi::channel_events::control_change(ch, 64, down ? 127 : 0);
  m.timestamp = 3;
  return m;
}

struct Rig
{
  Sustain node;
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;
  int cc64{};

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

  //! One tick; returns the pitches whose note-off came out.
  std::set<int> tick(std::vector<libremidi::message> in = {})
  {
    node.inputs.midi.midi_messages.assign(in.begin(), in.end());
    node.outputs.midi.midi_messages.clear();
    node(halp::tick{frames});
    std::set<int> ended;
    for(auto& m : node.outputs.midi.midi_messages)
    {
      if(m.timestamp < 0 || m.timestamp >= frames)
        errors.push_back("timestamp " + std::to_string(m.timestamp));
      if(m.get_message_type() == libremidi::message_type::CONTROL_CHANGE && m.bytes[1] == 64)
        cc64++;
      if(!mtk::is_note_on(m) && !mtk::is_note_off(m))
        continue;
      auto& n = sounding[{m.get_channel(), m.bytes[1]}];
      if(mtk::is_note_on(m))
      {
        if(n++ > 0)
          errors.push_back("started twice " + std::to_string(m.bytes[1]));
      }
      else
      {
        ended.insert(m.bytes[1]);
        if(n == 0)
          errors.push_back("stopped while silent " + std::to_string(m.bytes[1]));
        else
          n--;
      }
    }
    return ended;
  }

  int still_sounding() const
  {
    int total = 0;
    for(auto& [k, n] : sounding)
      total += n;
    return total;
  }

  void check_clean() const
  {
    INFO((errors.empty() ? std::string{} : errors.front()));
    CHECK(errors.empty());
    CHECK(still_sounding() == 0);
  }
};

using S = std::set<int>;
}

TEST_CASE("Midi sustain presets: every preset loads", "[presets][midi_sustain]")
{
  REQUIRE(presets().size() >= 5);
  for(auto& p : presets())
  {
    INFO(p.name.toStdString());
    Sustain s;
    const auto errors = midi_presets::apply(s.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
  }
}

TEST_CASE("Midi sustain presets: piano pedal", "[presets][midi_sustain]")
{
  Rig r{"Piano pedal"};
  // Without the pedal, notes end with their key
  r.tick({on(48)});
  CHECK(r.tick({off(48)}) == S{48});
  r.tick({on(60), pedal(true)});
  CHECK(r.tick({off(60)}).empty());
  r.tick({on(64)});
  CHECK(r.tick({off(64)}).empty());
  r.tick({on(67)});
  CHECK(r.tick({off(67)}).empty());
  // Every note rings until the pedal goes up
  CHECK(r.tick({pedal(false)}) == S{60, 64, 67});
  CHECK(r.cc64 == 2);
  r.check_clean();
}

TEST_CASE("Midi sustain presets: pedal without passing CC 64", "[presets][midi_sustain]")
{
  Rig r{"Pedal without passing CC 64"};
  r.tick({pedal(true), on(60)});
  CHECK(r.tick({off(60)}).empty());
  CHECK(r.tick({pedal(false)}) == S{60});
  CHECK(r.cc64 == 0);
  r.check_clean();
}

TEST_CASE("Midi sustain presets: chord pedal", "[presets][midi_sustain]")
{
  Rig r{"Chord pedal"};
  r.tick({pedal(true), on(60), on(64)});
  CHECK(r.tick({off(60), off(64)}).empty());
  // A new chord ends the previous one
  CHECK(r.tick({on(65), on(69)}) == S{60, 64});
  CHECK(r.tick({off(65), off(69)}).empty());
  CHECK(r.tick({pedal(false)}) == S{65, 69});
  r.check_clean();
}

TEST_CASE("Midi sustain presets: sostenuto pedal", "[presets][midi_sustain]")
{
  Rig r{"Sostenuto pedal"};
  r.tick({on(36)});
  r.tick({pedal(true)});
  // The bass held when the pedal went down rings
  CHECK(r.tick({off(36)}).empty());
  // What is played on top does not
  r.tick({on(60)});
  CHECK(r.tick({off(60)}) == S{60});
  CHECK(r.tick({pedal(false)}) == S{36});
  r.check_clean();
}

TEST_CASE("Midi sustain presets: semi sostenuto pedal", "[presets][midi_sustain]")
{
  Rig r{"Semi sostenuto pedal"};
  r.tick({on(36)});
  r.tick({pedal(true)});
  CHECK(r.tick({off(36)}).empty());
  r.tick({on(60), on(64)});
  CHECK(r.tick({off(60), off(64)}).empty());
  // The next chord ends the previous one, not the latched bass
  CHECK(r.tick({on(62), on(65)}) == S{60, 64});
  CHECK(r.tick({off(62), off(65)}).empty());
  CHECK(r.tick({pedal(false)}) == S{36, 62, 65});
  r.check_clean();
}

TEST_CASE("Midi sustain presets: drone, pedal always down", "[presets][midi_sustain]")
{
  Rig r{"Drone, pedal always down"};
  r.tick({on(36), on(43, 2)});
  CHECK(r.tick({off(36), off(43, 2)}).empty());
  r.tick({on(60)});
  CHECK(r.tick({off(60)}).empty());
  // A key struck again ends its held note first
  CHECK(r.tick({on(36)}) == S{36});
  CHECK(r.tick({off(36)}).empty());
  // An incoming pedal up does not lift the control
  CHECK(r.tick({pedal(false)}).empty());
  r.node.inputs.pedal.value = false;
  CHECK(r.tick() == S{36, 43, 60});
  r.check_clean();
}

TEST_CASE("Midi sustain presets: last chord rings, pedal always down", "[presets][midi_sustain]")
{
  Rig r{"Last chord rings, pedal always down"};
  r.tick({on(60), on(64), on(67)});
  CHECK(r.tick({off(60), off(64), off(67)}).empty());
  CHECK(r.tick({on(62), on(65), on(69)}) == S{60, 64, 67});
  CHECK(r.tick({off(62), off(65), off(69)}).empty());
  r.node.stop();
  CHECK(r.tick() == S{62, 65, 69});
  r.check_clean();
}
