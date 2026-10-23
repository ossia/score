// The Midi scaler presets of the default package load onto the object and do
// what their names say.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiScaler.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <map>
#include <set>
#include <vector>

using Scaler = mtk::MidiScaler;

namespace
{
const QString folder = "Midi scaler";
const QString uuid = "c43300b0-6c98-4a38-81f2-b549f0297e38";

struct out_msg
{
  int64_t at{};
  libremidi::message msg;
};

struct driver
{
  Scaler fx;
  int64_t now{};
  std::vector<out_msg> out;
  std::map<std::pair<int, int>, int> sounding;

  explicit driver(const QString& name)
  {
    fx.prepare(halp::setup{.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
    bool found = false;
    for(const auto& p : midi_presets::load(folder, uuid))
    {
      if(p.name != name)
        continue;
      found = true;
      const auto errors = midi_presets::apply(fx.inputs, p);
      INFO(errors.join("; ").toStdString());
      REQUIRE(errors.isEmpty());
    }
    INFO(name.toStdString());
    REQUIRE(found);
  }

  void raw(std::initializer_list<uint8_t> bytes, int ts = 0)
  {
    halp::midi_msg m;
    m.bytes.assign(bytes.begin(), bytes.end());
    m.timestamp = ts;
    fx.inputs.midi.push_back(m);
  }
  void note_on(int note, int ts = 0, int ch = 1)
  {
    raw({uint8_t(0x90 | (ch - 1)), uint8_t(note), 100}, ts);
  }
  void note_off(int note, int ts = 0, int ch = 1)
  {
    raw({uint8_t(0x80 | (ch - 1)), uint8_t(note), 0}, ts);
  }

  void tick(int frames = 512)
  {
    fx.outputs.midi.midi_messages.clear();
    fx(frames);
    int64_t last = 0;
    for(auto& m : fx.outputs.midi.midi_messages)
    {
      REQUIRE(m.timestamp >= 0);
      REQUIRE(m.timestamp < frames);
      REQUIRE(m.timestamp >= last);
      last = m.timestamp;
      if(mtk::is_note_on(m))
      {
        auto& s = sounding[{mtk::channel_of(m), m.bytes[1]}];
        REQUIRE(s == 0);
        s++;
      }
      else if(mtk::is_note_off(m))
      {
        auto& s = sounding[{mtk::channel_of(m), m.bytes[1]}];
        REQUIRE(s == 1);
        s--;
      }
      out.push_back({now + m.timestamp, m});
    }
    fx.inputs.midi.midi_messages.clear();
    now += frames;
  }

  int total_sounding() const
  {
    int n = 0;
    for(auto& [k, v] : sounding)
      n += v;
    return n;
  }

  std::vector<libremidi::message> of_kind(mtk::midi_kind k) const
  {
    std::vector<libremidi::message> r;
    for(auto& e : out)
      if(mtk::classify(e.msg) == k)
        r.push_back(e.msg);
    return r;
  }

  std::set<int> note_channels() const
  {
    std::set<int> r;
    for(auto& m : of_kind(mtk::midi_kind::note_on))
      r.insert(mtk::channel_of(m));
    return r;
  }

  //! Holds `notes`, then releases them a tick later.
  void chord(std::initializer_list<int> notes)
  {
    for(int n : notes)
      note_on(n);
    tick();
    for(int n : notes)
      note_off(n);
    tick();
    REQUIRE(total_sounding() == 0);
  }

  //! Pitch each note-on sounds at: its note plus the last bend of its channel.
  std::vector<double> sounding_pitches() const
  {
    const int range = fx.inputs.bend_range.value;
    std::map<int, int> bend;
    std::vector<double> r;
    for(auto& e : out)
    {
      const int ch = mtk::channel_of(e.msg);
      if(mtk::classify(e.msg) == mtk::midi_kind::pitch_bend)
        bend[ch] = e.msg.bytes[1] | (e.msg.bytes[2] << 7);
      else if(mtk::is_note_on(e.msg))
      {
        REQUIRE(bend.count(ch));
        r.push_back(e.msg.bytes[1] + (bend[ch] - 8192) / 8192. * range);
      }
    }
    return r;
  }

  std::vector<libremidi::message> sysex() const
  {
    std::vector<libremidi::message> r;
    for(auto& e : out)
      if(!e.msg.bytes.empty() && e.msg.bytes[0] == 0xF0)
        r.push_back(e.msg);
    return r;
  }

  //! Semitone and fraction (in semitones) sent for `key` by the MTS messages.
  double mts_pitch(int key) const
  {
    for(auto& m : sysex())
    {
      REQUIRE(m.bytes.size() == 8 + 4 * 4);
      REQUIRE(m.bytes[1] == 0x7F);
      REQUIRE(m.bytes[3] == 0x08);
      REQUIRE(m.bytes[4] == 0x02);
      for(int i = 0; i < m.bytes[6]; i++)
      {
        const auto* d = &m.bytes[7 + 4 * i];
        if(d[0] == key)
          return d[1] + ((d[2] << 7) | d[3]) / 16384.;
      }
    }
    FAIL("key not tuned");
    return 0.;
  }
};

bool near(double a, double b, double tol = 0.01)
{
  return std::abs(a - b) < tol;
}
}

TEST_CASE("Midi scaler presets: every preset keeps notes paired", "[presets][midi_scaler]")
{
  const auto presets = midi_presets::load(folder, uuid);
  REQUIRE(presets.size() >= 6);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    driver d{p.name};
    for(int i = 0; i < 20; i++)
    {
      d.note_on(48 + i % 24, 10);
      d.note_on(55 + i % 13, 300);
      d.tick();
      d.note_off(48 + i % 24, 20);
      d.note_off(55 + i % 13, 400);
      d.tick();
    }
    d.tick();
    CHECK(d.total_sounding() == 0);
    CHECK(d.of_kind(mtk::midi_kind::note_on).size() == 40);
  }
}

TEST_CASE("Midi scaler preset: MPE synth, bend 48", "[presets][midi_scaler]")
{
  driver d{"MPE synth, bend 48"};
  d.chord({60, 64, 67});
  const auto chans = d.note_channels();
  CHECK(chans.size() == 3);
  CHECK(!chans.contains(1));
  const auto pitches = d.sounding_pitches();
  REQUIRE(pitches.size() == 3);
  CHECK(near(pitches[0], 60));
  CHECK(near(pitches[1], 64));
  CHECK(near(pitches[2], 67));
}

TEST_CASE("Midi scaler preset: GM synth, drum channel skipped", "[presets][midi_scaler]")
{
  driver d{"GM synth, drum channel skipped"};
  for(int i = 0; i < 15; i++)
    d.note_on(40 + i);
  d.tick();
  CHECK(d.note_channels().size() == 15);
  CHECK(!d.note_channels().contains(10));
  for(int i = 0; i < 15; i++)
    d.note_off(40 + i);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi scaler preset: Channels 2 to 9 for a multitimbral synth", "[presets][midi_scaler]")
{
  driver d{"Channels 2 to 9 for a multitimbral synth"};
  d.chord({60, 62, 64, 65, 67, 69, 71, 72});
  CHECK(d.note_channels() == std::set<int>{2, 3, 4, 5, 6, 7, 8, 9});
  // Controllers reach every channel of the set.
  d.out.clear();
  d.raw({0xB0, 1, 64});
  d.tick();
  std::set<int> cc;
  for(auto& m : d.of_kind(mtk::midi_kind::control_change))
    cc.insert(mtk::channel_of(m));
  CHECK(cc == std::set<int>{2, 3, 4, 5, 6, 7, 8, 9});
}

TEST_CASE("Midi scaler preset: Four mono synths, bend 12", "[presets][midi_scaler]")
{
  driver d{"Four mono synths, bend 12"};
  for(int i = 0; i < 8; i++)
    d.chord({60});
  const auto chans = d.note_channels();
  CHECK(chans == std::set<int>{1, 2, 3, 4});
  // Always send bend: one bend before every note, even an unchanged one.
  CHECK(d.of_kind(mtk::midi_kind::pitch_bend).size() == 8);
  for(double p : d.sounding_pitches())
    CHECK(near(p, 60));
}

TEST_CASE("Midi scaler preset: MTS sysex", "[presets][midi_scaler]")
{
  driver d{"MTS sysex"};
  d.chord({60});
  CHECK(d.sysex().size() == 128 / Scaler::mts_keys_per_message);
  CHECK(near(d.mts_pitch(60), 60, 0.001));
  CHECK(near(d.mts_pitch(69), 69, 0.001));
  // Notes pass unchanged.
  const auto on = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(on.size() == 1);
  CHECK(mtk::channel_of(on[0]) == 1);
  CHECK(on[0].bytes[1] == 60);
  CHECK(d.of_kind(mtk::midi_kind::pitch_bend).empty());
}

TEST_CASE("Midi scaler preset: MTS sysex, octave up", "[presets][midi_scaler]")
{
  driver d{"MTS sysex, octave up"};
  d.chord({60});
  CHECK(near(d.mts_pitch(60), 72, 0.001));
  CHECK(near(d.mts_pitch(48), 60, 0.001));
}

TEST_CASE("Midi scaler preset: Beating detune 2 Hz, MPE synth", "[presets][midi_scaler]")
{
  driver d{"Beating detune 2 Hz, MPE synth"};
  d.chord({69});
  const auto pitches = d.sounding_pitches();
  REQUIRE(pitches.size() == 1);
  CHECK(near(pitches[0], 69. + 12. * std::log2(442. / 440.), 0.01));
  CHECK(!d.note_channels().contains(1));
}

TEST_CASE("Midi scaler preset: Octave down, MPE synth", "[presets][midi_scaler]")
{
  driver d{"Octave down, MPE synth"};
  d.chord({72, 76});
  const auto pitches = d.sounding_pitches();
  REQUIRE(pitches.size() == 2);
  CHECK(near(pitches[0], 60));
  CHECK(near(pitches[1], 64));
}
