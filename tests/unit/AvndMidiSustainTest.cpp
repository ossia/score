// Midi sustain (mtk::MidiSustain): the four pedal modes, re-struck notes, the
// pedal control, and the common checklist of the MIDI objects.

#include <Advanced/MidiScaler/MidiSustain.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Sustain = mtk::MidiSustain;

libremidi::message on(int ch, int p, int v = 100, int64_t ts = 0)
{
  auto m = libremidi::channel_events::note_on(ch, p, v);
  m.timestamp = ts;
  return m;
}
libremidi::message off(int ch, int p, int64_t ts = 0)
{
  auto m = libremidi::channel_events::note_off(ch, p, 0);
  m.timestamp = ts;
  return m;
}
libremidi::message pedal(int ch, bool down, int64_t ts = 0)
{
  auto m = libremidi::channel_events::control_change(ch, 64, down ? 127 : 0);
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
      errors.push_back("timestamp in an empty tick");
    if(!mtk::is_note_on(m) && !mtk::is_note_off(m))
      return;
    auto& n = sounding[{m.get_channel(), m.bytes[1]}];
    if(mtk::is_note_on(m))
    {
      if(n > 0)
        errors.push_back("started twice: " + std::to_string(m.bytes[1]));
      n++;
    }
    else
    {
      if(n == 0)
        errors.push_back("stopped while silent: " + std::to_string(m.bytes[1]));
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
  Sustain node;
  Listener out;
  std::vector<libremidi::message> last;

  explicit Rig(Sustain::Mode mode = Sustain::All) { node.inputs.mode.value = mode; }

  const std::vector<libremidi::message>& tick(std::vector<libremidi::message> in = {}, int frames = 64)
  {
    node.inputs.midi.midi_messages.assign(in.begin(), in.end());
    node.outputs.midi.midi_messages.clear();
    node(halp::tick{frames});
    last.assign(node.outputs.midi.midi_messages.begin(), node.outputs.midi.midi_messages.end());
    for(auto& m : last)
      out.feed(m, frames);
    return last;
  }

  std::vector<int> offs()
  {
    std::vector<int> r;
    for(auto& m : last)
      if(mtk::is_note_off(m))
        r.push_back(m.bytes[1]);
    std::sort(r.begin(), r.end());
    return r;
  }
};
}

TEST_CASE("Midi sustain: All holds every note until the pedal goes up", "[avnd][midi][sustain]")
{
  Rig r;
  // Without the pedal, notes pass.
  CHECK(r.tick({on(1, 60, 90, 3)}).size() == 1);
  CHECK(r.tick({off(1, 60, 4)}).size() == 1);

  auto& p = r.tick({pedal(1, true, 2)});
  REQUIRE(p.size() == 1); // passed on
  CHECK(p[0].timestamp == 2);
  r.tick({on(1, 60), on(1, 64)});
  CHECK(r.tick({off(1, 60), off(1, 64)}).empty());
  r.tick({on(1, 67)});
  CHECK(r.tick({off(1, 67)}).empty());
  CHECK(r.out.still_sounding() == 3);

  // The pedal of another channel does nothing here.
  r.tick({on(2, 50)});
  CHECK(r.tick({off(2, 50)}).size() == 1);

  r.tick({pedal(1, false, 9)});
  CHECK(r.offs() == std::vector<int>{60, 64, 67});
  for(auto& m : r.last)
    CHECK(m.timestamp == 9);
  // A note still held by its key keeps sounding.
  r.tick({pedal(1, true), on(1, 72)});
  r.tick({pedal(1, false)});
  CHECK(r.offs().empty());
  CHECK(r.tick({off(1, 72)}).size() == 1);
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi sustain: a held note struck again ends first", "[avnd][midi][sustain]")
{
  Rig r;
  r.tick({pedal(1, true), on(1, 60)});
  r.tick({off(1, 60)});
  auto& again = r.tick({on(1, 60, 80, 5)});
  REQUIRE(again.size() == 2);
  CHECK(mtk::is_note_off(again[0]));
  CHECK(mtk::is_note_on(again[1]));
  CHECK(again[1].bytes[2] == 80);
  CHECK(again[0].timestamp == 5);
  // Two note-ons without a release: the same.
  CHECK(r.tick({on(1, 60, 70)}).size() == 2);
  r.tick({off(1, 60)});
  r.tick({pedal(1, false)});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi sustain: Chord keeps only the last chord", "[avnd][midi][sustain]")
{
  Rig r{Sustain::Chord};
  r.tick({pedal(1, true), on(1, 60), on(1, 64)});
  r.tick({off(1, 60), off(1, 64)});
  CHECK(r.out.still_sounding() == 2);
  // A new chord ends the previous one.
  r.tick({on(1, 62)});
  CHECK(r.offs() == std::vector<int>{60, 64});
  // Legato: a key added while another is held is the same chord.
  r.tick({on(1, 65)});
  CHECK(r.offs().empty());
  r.tick({off(1, 62)});
  r.tick({on(1, 69)});
  CHECK(r.offs().empty());
  r.tick({off(1, 65), off(1, 69)});
  CHECK(r.out.still_sounding() == 3);
  r.tick({pedal(1, false)});
  CHECK(r.offs() == std::vector<int>{62, 65, 69});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi sustain: Sostenuto holds the notes down when the pedal went down", "[avnd][midi][sustain]")
{
  Rig r{Sustain::Sostenuto};
  r.tick({on(1, 48), on(1, 52)});
  r.tick({pedal(1, true)});
  r.tick({off(1, 48), off(1, 52)});
  CHECK(r.offs().empty());
  // Played after: not held.
  r.tick({on(1, 72)});
  r.tick({off(1, 72)});
  CHECK(r.offs() == std::vector<int>{72});
  r.tick({pedal(1, false)});
  CHECK(r.offs() == std::vector<int>{48, 52});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi sustain: Semi-sostenuto", "[avnd][midi][sustain]")
{
  Rig r{Sustain::SemiSostenuto};
  r.tick({on(1, 36)});
  r.tick({pedal(1, true)});
  r.tick({off(1, 36)});
  r.tick({on(1, 60), on(1, 64)});
  r.tick({off(1, 60), off(1, 64)});
  CHECK(r.out.still_sounding() == 3);
  // A new chord ends the previous chord, not the bass held at pedal down.
  r.tick({on(1, 62)});
  CHECK(r.offs() == std::vector<int>{60, 64});
  r.tick({off(1, 62)});
  r.tick({pedal(1, false)});
  CHECK(r.offs() == std::vector<int>{36, 62});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi sustain: the Pedal control and CC 64 options", "[avnd][midi][sustain]")
{
  Rig r;
  r.node.inputs.pedal.value = true;
  r.tick({on(3, 60), on(9, 61)});
  CHECK(r.tick({off(3, 60), off(9, 61)}).empty());
  // CC 64 up does not lift the control.
  r.tick({pedal(3, true), pedal(3, false)});
  CHECK(r.offs().empty());
  r.node.inputs.pedal.value = false;
  r.tick();
  CHECK(r.offs() == std::vector<int>{60, 61});

  // Swallowing CC 64.
  r.node.inputs.pass_pedal.value = false;
  CHECK(r.tick({pedal(1, true)}).empty());
  r.tick({on(1, 40)});
  r.tick({off(1, 40)});
  CHECK(r.offs().empty());
  // Reset all controllers lifts it.
  r.tick({libremidi::channel_events::control_change(1, 121, 0)});
  CHECK(r.offs() == std::vector<int>{40});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi sustain: all notes off, pass through, stop", "[avnd][midi][sustain]")
{
  Rig r;
  r.tick({pedal(1, true), on(1, 60), on(1, 62), on(2, 70)});
  r.tick({off(1, 60)});
  auto& panic = r.tick({libremidi::channel_events::control_change(1, 123, 0)});
  REQUIRE(panic.size() == 3);
  CHECK(panic[2].bytes[1] == 123);
  // The key's own note-off after the panic has nothing to end.
  CHECK(r.tick({off(1, 62)}).empty());
  CHECK(r.out.still_sounding() == 1);

  auto cc = libremidi::channel_events::control_change(1, 7, 99);
  cc.timestamp = 4;
  auto bend = libremidi::channel_events::pitch_bend(2, 300);
  bend.timestamp = 5;
  auto& pass = r.tick({cc, bend});
  REQUIRE(pass.size() == 2);
  CHECK(pass[0].bytes == cc.bytes);
  CHECK(pass[0].timestamp == 4);
  CHECK(pass[1].bytes == bend.bytes);

  r.tick({off(2, 70)});
  r.tick({on(2, 71)});
  r.node.stop();
  r.tick({}, 0);
  CHECK(r.out.still_sounding() == 0);
  // The pedal is up after a stop.
  r.tick({on(1, 50)});
  CHECK(r.tick({off(1, 50)}).size() == 1);

  // A note-on with velocity 0 is a release.
  r.tick({pedal(1, true), on(1, 55)});
  CHECK(r.tick({on(1, 55, 0)}).empty());
  r.tick({pedal(1, false)});
  CHECK(r.offs() == std::vector<int>{55});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi sustain: random playing never leaves a note behind", "[avnd][midi][sustain]")
{
  for(unsigned seed = 1; seed <= 20; seed++)
  {
    std::mt19937 rng{seed};
    auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>{lo, hi}(rng); };
    Rig r;
    std::map<std::pair<int, int>, bool> held;
    for(int step = 0; step < 500; step++)
    {
      std::vector<libremidi::message> in;
      for(int i = pick(0, 4); i > 0; i--)
      {
        const int ch = pick(1, 2), p = pick(40, 60);
        const int ts = pick(0, 63);
        switch(pick(0, 5))
        {
          case 0:
          case 1:
            in.push_back(on(ch, p, pick(1, 127), ts));
            held[{ch, p}] = true;
            break;
          case 2:
            in.push_back(off(ch, p, ts));
            held[{ch, p}] = false;
            break;
          case 3:
            in.push_back(on(ch, p, 0, ts));
            held[{ch, p}] = false;
            break;
          case 4:
            in.push_back(pedal(ch, pick(0, 1), ts));
            break;
          case 5:
            if(pick(0, 10) == 0)
              in.push_back(libremidi::channel_events::control_change(ch, 123, 0));
            break;
        }
      }
      std::sort(in.begin(), in.end(), [](auto& a, auto& b) { return a.timestamp < b.timestamp; });
      switch(pick(0, 20))
      {
        case 0: r.node.inputs.mode.value = Sustain::Mode(pick(0, 3)); break;
        case 1: r.node.inputs.pedal.value = !r.node.inputs.pedal.value; break;
        case 2: r.node.inputs.pass_pedal.value = !r.node.inputs.pass_pedal.value; break;
        case 3: r.node.stop(); break;
        default: break;
      }
      r.tick(std::move(in), pick(0, 20) == 0 ? 0 : 64);
    }
    std::vector<libremidi::message> release;
    for(auto& [k, h] : held)
      if(h)
        release.push_back(off(k.first, k.second));
    release.push_back(pedal(1, false));
    release.push_back(pedal(2, false));
    r.node.inputs.pedal.value = false;
    r.tick(std::move(release));

    INFO("seed " << seed << ": " << r.out.errors.size() << " errors, first: "
                 << (r.out.errors.empty() ? "" : r.out.errors[0]));
    CHECK(r.out.errors.empty());
    CHECK(r.out.still_sounding() == 0);
  }
}
