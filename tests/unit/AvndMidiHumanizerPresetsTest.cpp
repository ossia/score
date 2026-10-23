// The Midi Humanize presets of the default package load onto the object and
// keep every note paired.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiHumanizer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>

TEST_CASE("Midi Humanize presets", "[presets][midi_humanize]")
{
  const auto presets
      = midi_presets::load("Midi Humanize", "b9830e77-82fd-4d45-8b24-f78e5608eb21");
  REQUIRE(!presets.empty());

  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    mtk::MidiHumanizer fx;
    fx.prepare(halp::setup{.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
    const auto errors = midi_presets::apply(fx.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());

    std::map<int, int> sounding;
    int64_t pos = 0;
    for(int block = 0; block < 400; block++)
    {
      if(block % 8 == 0 && block < 300)
        fx.inputs.midi.push_back(libremidi::channel_events::note_on(1, 48 + block % 24, 100));
      if(block % 8 == 4 && block < 304)
        fx.inputs.midi.push_back(libremidi::channel_events::note_off(1, 48 + (block - 4) % 24, 0));
      fx(halp::tick_musical{.frames = 512, .position_in_frames = pos});
      pos += 512;
      for(auto& m : fx.outputs.midi.midi_messages)
      {
        if(m.get_message_type() == libremidi::message_type::NOTE_ON && m.bytes[2] > 0)
          sounding[m.bytes[1]]++;
        else if(m.get_message_type() == libremidi::message_type::NOTE_OFF
                || m.get_message_type() == libremidi::message_type::NOTE_ON)
          sounding[m.bytes[1]]--;
      }
      fx.outputs.midi.midi_messages.clear();
      fx.inputs.midi.midi_messages.clear();
    }
    for(auto [pitch, n] : sounding)
    {
      INFO("pitch " << pitch);
      CHECK(n == 0);
    }
  }
}

// ------------------------------------------------------------------------
// The presets of the chance, euclid and controller features: what their names
// say.

#include "MidiPresetRig.hpp"

#include <set>

namespace
{
using HRig = midi_presets::rig<mtk::MidiHumanizer>;
const QString humanize_uuid = "b9830e77-82fd-4d45-8b24-f78e5608eb21";

void setup(HRig& r, const char* name)
{
  const auto errors = r.load(midi_presets::find("Midi Humanize", humanize_uuid, name));
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
}

void finish(HRig& r, int64_t until, const HRig::input& in)
{
  r.run(until, in);
  r.stop();
  r.check_clean();
}

//! Notes of pitch `pitch` every `step` frames, each `step / 2` long.
HRig::input pulse(int count, int64_t step, int pitch = 60, int vel = 100)
{
  HRig::input in;
  for(int i = 0; i < count; i++)
  {
    in.push_back({i * step, midi_presets::on(pitch, vel)});
    in.push_back({i * step + step / 2, midi_presets::off(pitch)});
  }
  return in;
}
}

TEST_CASE("Midi Humanize preset: Chord dropouts", "[presets][midi_humanize]")
{
  using namespace midi_presets;
  HRig r;
  setup(r, "Chord dropouts");
  const int chords = 300;
  HRig::input in;
  for(int i = 0; i < chords; i++)
  {
    const int64_t t = i * quarter;
    // A slightly rolled chord: inside the chord window
    in.push_back({t, on(60)});
    in.push_back({t + 200, on(64)});
    in.push_back({t + 400, on(67)});
    for(int p : {60, 64, 67})
      in.push_back({t + quarter / 2, off(p)});
  }
  finish(r, chords * quarter + bar, in);

  // A chord plays whole or not at all
  std::map<int64_t, int> per_chord;
  for(auto& e : r.ons())
    per_chord[e.at / quarter]++;
  for(auto& [c, n] : per_chord)
  {
    INFO("chord " << c);
    CHECK(n == 3);
  }
  const double played = double(per_chord.size()) / chords;
  CHECK(played > 0.6);
  CHECK(played < 0.8);
  // Nothing else is touched
  for(auto& e : r.ons())
  {
    CHECK(e.data2 == 100);
    CHECK(e.at % quarter == (e.data1 == 60 ? 0 : e.data1 == 64 ? 200 : 400));
  }
}

TEST_CASE("Midi Humanize preset: Bar dropouts", "[presets][midi_humanize]")
{
  using namespace midi_presets;
  HRig r;
  setup(r, "Bar dropouts");
  const int bars = 120;
  finish(r, (bars + 1) * bar, pulse(bars * 8, 2 * sixteenth));

  // Each bar plays its eight eighths, or is silent
  std::map<int64_t, int> per_bar;
  for(auto& e : r.ons())
  {
    per_bar[e.at / bar]++;
    CHECK(e.at % (2 * sixteenth) == 0);
  }
  for(auto& [b, n] : per_bar)
  {
    INFO("bar " << b);
    CHECK(n == 8);
  }
  const double played = double(per_bar.size()) / bars;
  CHECK(played > 0.6);
  CHECK(played < 0.9);
}

TEST_CASE("Midi Humanize preset: Velocity as chance", "[presets][midi_humanize]")
{
  using namespace midi_presets;
  HRig r;
  setup(r, "Velocity as chance");
  const int n = 400;
  HRig::input in;
  for(int i = 0; i < n; i++)
  {
    // A loud and a ghost note, on two keys
    in.push_back({i * quarter, on(36, 127)});
    in.push_back({i * quarter + 1000, off(36)});
    in.push_back({i * quarter + quarter / 2, on(42, 32)});
    in.push_back({i * quarter + quarter / 2 + 1000, off(42)});
  }
  finish(r, n * quarter + bar, in);
  int loud = 0, ghost = 0;
  for(auto& e : r.ons())
  {
    (e.data1 == 36 ? loud : ghost)++;
    CHECK(e.data2 == (e.data1 == 36 ? 127 : 32));
  }
  CHECK(loud == n);
  // 32 / 127: about a quarter of the ghost notes
  CHECK(ghost > n * 0.17);
  CHECK(ghost < n * 0.34);
}

TEST_CASE("Midi Humanize preset: Euclid 3 of 8", "[presets][midi_humanize]")
{
  using namespace midi_presets;
  HRig r;
  setup(r, "Euclid 3 of 8");
  const int64_t eighth = 2 * sixteenth;
  finish(r, 5 * bar, pulse(32, eighth));
  std::vector<int64_t> want;
  for(int i = 0; i < 32; i++)
    if(std::set<int>{0, 3, 6}.count(i % 8))
      want.push_back(i * eighth);
  CHECK(r.on_dates() == want);
}

TEST_CASE("Midi Humanize preset: Euclid 5 of 16", "[presets][midi_humanize]")
{
  using namespace midi_presets;
  HRig r;
  setup(r, "Euclid 5 of 16");
  finish(r, 5 * bar, pulse(64, sixteenth, 42));
  std::vector<int64_t> want;
  for(int i = 0; i < 64; i++)
    if(std::set<int>{0, 4, 7, 10, 13}.count(i % 16))
      want.push_back(i * sixteenth);
  CHECK(r.on_dates() == want);
}

TEST_CASE("Midi Humanize preset: Random brightness CC 74", "[presets][midi_humanize]")
{
  using namespace midi_presets;
  HRig r;
  setup(r, "Random brightness CC 74");
  const int n = 300;
  HRig::input in = pulse(8, quarter);
  for(int i = 0; i < n; i++)
  {
    in.push_back({i * 600 + 10, cc(74, 64)});
    in.push_back({i * 600 + 20, cc(1, 64)});
  }
  finish(r, 3 * bar, in);

  std::set<int> brightness;
  double sum = 0;
  int count74 = 0, count1 = 0;
  for(auto& e : r.events)
  {
    if(e.status != 0xB0)
      continue;
    if(e.data1 == 74)
    {
      CHECK(e.data2 >= 44);
      CHECK(e.data2 <= 84);
      brightness.insert(e.data2);
      sum += e.data2;
      count74++;
    }
    else if(e.data1 == 1)
    {
      CHECK(e.data2 == 64);
      count1++;
    }
  }
  CHECK(count74 == n);
  CHECK(count1 == n);
  CHECK(brightness.size() > 10);
  CHECK(std::abs(sum / n - 64.) < 4.);
  // The notes are untouched
  CHECK(r.ons().size() == 8);
  for(auto& e : r.ons())
    CHECK(e.at % quarter == 0);
}
