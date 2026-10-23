// Midi scale (Nodes::MidiUtil::Node): every note it starts, it stops, on the
// channel it came in on, whatever the input and the control changes do.

#include <Fx/MidiUtil.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <random>
#include <utility>
#include <vector>

namespace
{
using Scale = Nodes::MidiUtil::Node;

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

//! Follows what the output plays, and fails on what a synth would get wrong:
//! a note started twice, a note stopped that does not play.
struct Listener
{
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;

  void feed(const libremidi::message& m)
  {
    const auto type = m.get_message_type();
    if(type != libremidi::message_type::NOTE_ON
       && type != libremidi::message_type::NOTE_OFF)
      return;
    const int chan = m.get_channel();
    const int pitch = m.bytes[1];
    const bool on = type == libremidi::message_type::NOTE_ON && m.bytes[2] > 0;
    if(pitch > 127)
      errors.push_back("pitch " + std::to_string(pitch));
    auto& n = sounding[{chan, pitch}];
    if(on)
    {
      if(n > 0)
        errors.push_back(
            "started twice: ch " + std::to_string(chan) + " " + std::to_string(pitch));
      n++;
    }
    else
    {
      if(n == 0)
        errors.push_back(
            "stopped while silent: ch " + std::to_string(chan) + " "
            + std::to_string(pitch));
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
  Scale node;
  Listener out;
  std::vector<libremidi::message> last;

  Rig(std::string scale = "ionian", int base = 0, int transpose = 0)
  {
    node.inputs.sc.value = std::move(scale);
    node.inputs.base.value = base;
    node.inputs.transp.value = transpose;
  }

  //! One tick with these input messages; returns what came out.
  const std::vector<libremidi::message>& tick(std::vector<libremidi::message> in = {})
  {
    node.inputs.midi.midi_messages.assign(in.begin(), in.end());
    node.outputs.midi.midi_messages.clear();
    node(halp::tick_flicks{});
    last.assign(node.outputs.midi.begin(), node.outputs.midi.end());
    for(auto& m : last)
      out.feed(m);
    return last;
  }
};
}

TEST_CASE("Midi scale: a note maps into the scale on its own channel", "[fx][midi][scale]")
{
  Rig r{"ionian"};
  auto& on = r.tick({note_on(1, 61, 100, 7)});
  REQUIRE(on.size() == 1);
  // C# is not in C major: the closest note above, D.
  CHECK(on[0].bytes[0] == 0x90);
  CHECK(on[0].bytes[1] == 62);
  CHECK(on[0].bytes[2] == 100);
  CHECK(on[0].timestamp == 7);

  auto& off = r.tick({note_off(1, 61, 3)});
  REQUIRE(off.size() == 1);
  CHECK(off[0].bytes[0] == 0x80);
  CHECK(off[0].bytes[1] == 62);
  CHECK(off[0].timestamp == 3);
  CHECK(r.out.errors.empty());
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Midi scale: every channel keeps its notes", "[fx][midi][scale]")
{
  Rig r{"ionian"};
  for(int ch = 1; ch <= 16; ch++)
    r.tick({note_on(ch, 60, 90)});
  for(auto& [k, n] : r.out.sounding)
    if(n)
      CHECK(k.second == 60);
  for(int ch = 1; ch <= 16; ch++)
    r.tick({note_off(ch, 60)});
  INFO(r.out.errors.size() << " errors, first: "
                           << (r.out.errors.empty() ? "" : r.out.errors[0]));
  CHECK(r.out.errors.empty());
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Midi scale: a note on with velocity 0 releases the key", "[fx][midi][scale]")
{
  Rig r{"ionian"};
  r.tick({note_on(1, 64, 100)});
  r.tick({note_on(1, 64, 0)});
  CHECK(r.out.still_sounding() == 0);
  // And a scale change afterwards brings nothing back.
  r.node.inputs.transp.value = 5;
  CHECK(r.tick().empty());
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi scale: all notes off forgets the keys held on that channel", "[fx][midi][scale]")
{
  Rig r{"ionian"};
  r.tick({note_on(1, 61, 100), note_on(2, 61, 100)});
  // The synth drops everything on channel 1: so does the scale, and a later
  // transposition brings nothing back on it.
  auto all_notes_off = libremidi::channel_events::control_change(1, 123, 0);
  const auto& out = r.tick({all_notes_off});
  REQUIRE(out.size() == 1);
  CHECK(out[0].bytes[1] == 123);
  r.node.inputs.transp.value = 2;
  const auto& moved = r.tick();
  REQUIRE(moved.size() == 2);
  CHECK(moved[0].get_channel() == 2);
  CHECK(moved[1].get_channel() == 2);
  r.tick({note_off(2, 61)});
  // The listener still counts channel 1's note: the synth got the panic.
  CHECK(r.out.sounding[{2, 62}] == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi scale: the same key on two channels, and pressed again", "[fx][midi][scale]")
{
  Rig r{"ionian"};
  r.tick({note_on(1, 60, 100), note_on(2, 60, 100)});
  r.tick({note_on(1, 60, 80)});  // pressed again without a release
  r.tick({note_off(1, 60), note_off(2, 60)});
  CHECK(r.out.errors.empty());
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Midi scale: two keys on one note of the scale", "[fx][midi][scale]")
{
  Rig r{"ionian"};
  // C# and D both play D.
  r.tick({note_on(1, 61, 100), note_on(1, 62, 100)});
  r.tick({note_off(1, 61)});
  CHECK(r.out.still_sounding() == 1);  // D is still held
  r.tick({note_off(1, 62)});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi scale: held notes follow scale, base and transposition", "[fx][midi][scale]")
{
  Rig r{"ionian"};
  r.tick({note_on(1, 120, 100), note_on(1, 61, 100)});
  r.node.inputs.transp.value = 48;  // 120 + 48 is past 127
  r.tick();
  r.node.inputs.base.value = 12;    // the top of the Base slider
  r.tick();
  r.node.inputs.sc.value = "III";
  r.tick();
  r.node.inputs.sc.value = "000000000000";  // a scale with no note
  r.tick();
  r.tick({note_off(1, 120), note_off(1, 61)});
  INFO(r.out.errors.size() << " errors, first: "
                           << (r.out.errors.empty() ? "" : r.out.errors[0]));
  CHECK(r.out.errors.empty());
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Midi scale: a scale with no note plays nothing", "[fx][midi][scale]")
{
  Rig r{"000000000000"};
  CHECK(r.tick({note_on(1, 60, 100), note_on(1, 127, 100), note_on(1, 0, 100)}).empty());
  CHECK(r.tick({note_off(1, 60), note_off(1, 127), note_off(1, 0)}).empty());
}

TEST_CASE("Midi scale: random playing never leaves a note behind", "[fx][midi][scale]")
{
  const char* scales[] = {"all", "ionian", "dorian", "locrian", "I", "V", "VII",
                          "101010101010", "100000000000", ""};
  for(unsigned seed = 1; seed <= 20; seed++)
  {
    std::mt19937 rng{seed};
    auto pick = [&](int lo, int hi) {
      return std::uniform_int_distribution<int>{lo, hi}(rng);
    };
    Rig r{"ionian"};
    std::map<std::pair<int, int>, bool> held;
    for(int step = 0; step < 400; step++)
    {
      std::vector<libremidi::message> in;
      for(int i = pick(0, 4); i > 0; i--)
      {
        const int ch = pick(1, 3), p = pick(0, 127);
        switch(pick(0, 3))
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
        }
      }
      if(pick(0, 9) == 0)
        r.node.inputs.sc.value = scales[pick(0, 9)];
      if(pick(0, 9) == 0)
        r.node.inputs.base.value = pick(0, 12);
      if(pick(0, 9) == 0)
        r.node.inputs.transp.value = pick(-48, 48);
      r.tick(std::move(in));
    }
    std::vector<libremidi::message> release;
    for(auto& [k, h] : held)
      if(h)
        release.push_back(note_off(k.first, k.second));
    r.tick(std::move(release));

    INFO("seed " << seed << ": " << r.out.errors.size() << " errors, first: "
                 << (r.out.errors.empty() ? "" : r.out.errors[0]));
    CHECK(r.out.errors.empty());
    CHECK(r.out.still_sounding() == 0);
  }
}

TEST_CASE("Midi scale: direction of out-of-scale notes", "[fx][midi][scale]")
{
  using Nodes::MidiUtil::scale_direction;
  auto first = [](Rig& r, int pitch) {
    auto& on = r.tick({note_on(1, pitch, 100)});
    const int out = on.empty() ? -1 : on[0].bytes[1];
    r.tick({note_off(1, pitch)});
    return out;
  };

  Rig r{"ionian"};
  CHECK(first(r, 61) == 62); // Nearest, the default: ties go up
  CHECK(first(r, 66) == 67);
  r.node.inputs.direction.value = scale_direction::down;
  CHECK(first(r, 61) == 60);
  CHECK(first(r, 66) == 65);
  CHECK(first(r, 64) == 64);
  r.node.inputs.direction.value = scale_direction::up;
  CHECK(first(r, 63) == 64);
  CHECK(first(r, 66) == 67);

  // Nothing that way: the other way.
  Rig c{"100000000000"};
  c.node.inputs.direction.value = scale_direction::up;
  CHECK(first(c, 127) == 120);
  c.node.inputs.direction.value = scale_direction::down;
  CHECK(first(c, 5) == 0);
  CHECK(first(c, 13) == 12);
  // A custom mask.
  c.node.inputs.sc.value = "001000010000"; // D and G
  CHECK(first(c, 60) == 55);
  c.node.inputs.direction.value = scale_direction::up;
  CHECK(first(c, 60) == 62);
  CHECK(r.out.errors.empty());
  CHECK(c.out.errors.empty());
}

TEST_CASE("Midi scale: per-degree remap", "[fx][midi][scale]")
{
  using Nodes::MidiUtil::scale_mode;
  Rig r{"ionian"};
  // Not used unless the mode says so.
  r.node.inputs.map.value[1] = 0;
  CHECK(r.tick({note_on(1, 61, 100)})[0].bytes[1] == 62);
  r.tick({note_off(1, 61)});

  r.node.inputs.mode.value = scale_mode::remap;
  r.node.inputs.map.value[4] = 3; // major third to minor third
  CHECK(r.tick({note_on(1, 61, 100)})[0].bytes[1] == 60);
  CHECK(r.tick({note_on(1, 76, 100)})[0].bytes[1] == 75);
  // Degrees mapped to themselves, and out-of-scale notes, are left as they are.
  CHECK(r.tick({note_on(1, 66, 100)})[0].bytes[1] == 66);
  r.tick({note_off(1, 61), note_off(1, 76), note_off(1, 66)});
  CHECK(r.out.still_sounding() == 0);

  // Degrees count from Base; Transpose still applies.
  r.node.inputs.base.value = 2;
  r.node.inputs.transp.value = 12;
  CHECK(r.tick({note_on(1, 66, 100)})[0].bytes[1] == 77); // F# is III of D
  r.tick({note_off(1, 66)});

  // The scale does not apply: an empty one still plays.
  r.node.inputs.sc.value = "000000000000";
  r.node.inputs.transp.value = 0;
  CHECK(r.tick({note_on(1, 60, 100)})[0].bytes[1] == 60);
  r.tick({note_off(1, 60)});
  // Clamped at the ends.
  r.node.inputs.base.value = 0;
  r.node.inputs.map.value[7] = 12;
  CHECK(r.tick({note_on(1, 127, 100)})[0].bytes[1] == 127);
  r.tick({note_off(1, 127)});
  CHECK(r.out.errors.empty());
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Midi scale: muted degrees, fold and range", "[fx][midi][scale]")
{
  using Nodes::MidiUtil::scale_mode;
  Rig r{"all"};
  r.node.inputs.mode.value = scale_mode::remap;

  // A muted degree plays nothing, in every octave
  r.node.inputs.map.value[1] = -1;
  CHECK(r.tick({note_on(1, 61, 100)}).empty());
  CHECK(r.tick({note_on(1, 37, 100)}).empty());
  r.tick({note_off(1, 61), note_off(1, 37)});
  CHECK(r.tick({note_on(1, 62, 100)})[0].bytes[1] == 62);
  r.tick({note_off(1, 62)});

  // The root to the octave above; folded, it stays
  r.node.inputs.map.value[0] = 12;
  CHECK(r.tick({note_on(1, 60, 100)})[0].bytes[1] == 72);
  r.tick({note_off(1, 60)});
  r.node.inputs.fold.value = true;
  CHECK(r.tick({note_on(1, 60, 100)})[0].bytes[1] == 60);
  r.tick({note_off(1, 60)});
  // E to the C above (+8) goes to the C below (-4)
  r.node.inputs.map.value[4] = 12;
  CHECK(r.tick({note_on(1, 64, 100)})[0].bytes[1] == 60);
  r.tick({note_off(1, 64)});

  // Outside [Lowest, Lowest + Range] notes pass as they are, untransposed
  r.node.inputs.transp.value = 7;
  r.node.inputs.lowest.value = 60;
  r.node.inputs.range.value = 11;
  CHECK(r.tick({note_on(1, 59, 100)})[0].bytes[1] == 59);
  CHECK(r.tick({note_on(1, 72, 100)})[0].bytes[1] == 72);
  CHECK(r.tick({note_on(1, 62, 100)})[0].bytes[1] == 69);
  CHECK(r.tick({note_on(1, 61, 100)}).empty());
  r.tick({note_off(1, 59), note_off(1, 72), note_off(1, 62), note_off(1, 61)});
  CHECK(r.out.errors.empty());
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Midi scale: held notes follow mode, direction and degrees", "[fx][midi][scale]")
{
  using Nodes::MidiUtil::scale_direction;
  using Nodes::MidiUtil::scale_mode;
  Rig r{"ionian"};
  r.tick({note_on(1, 64, 100), note_on(1, 61, 100)});
  r.node.inputs.mode.value = scale_mode::remap;
  r.node.inputs.map.value[4] = 3;
  auto& moved = r.tick();
  REQUIRE(moved.size() == 4); // 64 -> 63 and 62 -> 61
  CHECK(r.out.sounding[{1, 63}] == 1);
  CHECK(r.out.sounding[{1, 61}] == 1);
  r.node.inputs.mode.value = scale_mode::scale;
  r.node.inputs.direction.value = scale_direction::down;
  r.tick();
  CHECK(r.out.sounding[{1, 64}] == 1);
  CHECK(r.out.sounding[{1, 60}] == 1);
  r.tick({note_off(1, 64), note_off(1, 61)});
  CHECK(r.out.errors.empty());
  CHECK(r.out.still_sounding() == 0);
}

TEST_CASE("Midi scale: random playing with remap and directions", "[fx][midi][scale]")
{
  using Nodes::MidiUtil::scale_direction;
  using Nodes::MidiUtil::scale_mode;
  for(unsigned seed = 1; seed <= 20; seed++)
  {
    std::mt19937 rng{seed};
    auto pick = [&](int lo, int hi) {
      return std::uniform_int_distribution<int>{lo, hi}(rng);
    };
    Rig r{"dorian"};
    std::map<std::pair<int, int>, bool> held;
    for(int step = 0; step < 400; step++)
    {
      std::vector<libremidi::message> in;
      for(int i = pick(0, 4); i > 0; i--)
      {
        const int ch = pick(1, 3), p = pick(0, 127);
        if(pick(0, 1))
        {
          in.push_back(note_on(ch, p, pick(1, 127), pick(0, 63)));
          held[{ch, p}] = true;
        }
        else
        {
          in.push_back(note_off(ch, p, pick(0, 63)));
          held[{ch, p}] = false;
        }
      }
      auto& n = r.node.inputs;
      switch(pick(0, 12))
      {
        case 0: n.mode.value = scale_mode(pick(0, 1)); break;
        case 1: n.direction.value = scale_direction(pick(0, 2)); break;
        case 2: n.map.value[0] = pick(-1, 12); break;
        case 3: n.map.value[4] = pick(-1, 12); break;
        case 4: n.map.value[11] = pick(-1, 12); break;
        case 8: n.fold.value = !n.fold.value; break;
        case 9: n.lowest.value = pick(0, 127); break;
        case 10: n.range.value = pick(0, 127); break;
        case 5: n.base.value = pick(0, 12); break;
        case 6: n.transp.value = pick(-48, 48); break;
        case 7: n.sc.value = pick(0, 1) ? "101000000001" : "lydian"; break;
        default: break;
      }
      r.tick(std::move(in));
    }
    std::vector<libremidi::message> release;
    for(auto& [k, h] : held)
      if(h)
        release.push_back(note_off(k.first, k.second));
    r.tick(std::move(release));

    INFO("seed " << seed << ": " << r.out.errors.size() << " errors, first: "
                 << (r.out.errors.empty() ? "" : r.out.errors[0]));
    CHECK(r.out.errors.empty());
    CHECK(r.out.still_sounding() == 0);
  }
}

TEST_CASE("halp's MIDI output counts channels from 1, as libremidi", "[fx][midi]")
{
  halp::midi_out_bus<"out", libremidi::message> bus;
  CHECK(bus.note_on(1, 60, 100).bytes[0] == 0x90);
  CHECK(bus.note_off(16, 60, 0).bytes[0] == 0x8F);
  CHECK(bus.note_on(1, 60, 100).get_channel() == 1);
}
