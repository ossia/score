#include <Advanced/MidiScaler/MidiScaler.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <map>
#include <string_view>
#include <vector>

using Catch::Approx;
using Scaler = mtk::MidiScaler;

namespace
{
// Pythagorean diatonic scale: every degree is off the 12-TET grid.
constexpr std::string_view pythagorean = R"(! pyth.scl
Pythagorean diatonic
7
9/8
81/64
4/3
3/2
27/16
243/128
2/1
)";

constexpr std::string_view edo19 = R"(! 19edo.scl
19 equal
19
63.15789
126.31579
189.47368
252.63158
315.78947
378.94737
442.10526
505.26316
568.42105
631.57895
694.73684
757.89474
821.05263
884.21053
947.36842
1010.52632
1073.68421
1136.84211
2/1
)";

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
  //! Notes sounding at the output, per channel and pitch.
  std::map<std::pair<int, int>, int> sounding;

  driver()
  {
    fx.prepare(halp::setup{.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  }

  void raw(std::initializer_list<uint8_t> bytes, int ts = 0)
  {
    halp::midi_msg m;
    m.bytes.assign(bytes.begin(), bytes.end());
    m.timestamp = ts;
    fx.inputs.midi.push_back(m);
  }
  void note_on(int note, int vel = 100, int ts = 0, int ch = 1)
  {
    raw({uint8_t(0x90 | (ch - 1)), uint8_t(note), uint8_t(vel)}, ts);
  }
  void note_off(int note, int ts = 0, int ch = 1)
  {
    raw({uint8_t(0x80 | (ch - 1)), uint8_t(note), 0}, ts);
  }

  void load_scale(std::string_view scl)
  {
    fx.inputs.scala.file.bytes = scl;
    fx.inputs.scala.update(fx);
  }

  void tick(int frames = 512)
  {
    fx.outputs.midi.midi_messages.clear();
    fx(frames);
    for(auto& m : fx.outputs.midi.midi_messages)
    {
      if(frames > 0)
      {
        REQUIRE(m.timestamp >= 0);
        REQUIRE(m.timestamp < frames);
      }
      else
        REQUIRE(m.timestamp == 0);
      if(mtk::is_note_on(m))
      {
        auto& s = sounding[{mtk::channel_of(m), m.bytes[1]}];
        REQUIRE(s == 0); // never twice the same note on the same channel
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
};

int bend_of(const libremidi::message& m)
{
  return m.bytes[1] | (m.bytes[2] << 7);
}

double target(const Scaler& fx, int key)
{
  return 12. * std::log2(fx.tuning.frequencyForMidiNote(key) / Tunings::MIDI_0_FREQ);
}

//! For each note-on in `out`, the pitch it actually sounds at: its note plus
//! the last bend sent on its channel before it.
std::vector<double> sounding_pitches(const std::vector<out_msg>& out, int range)
{
  std::map<int, int> bend;
  std::vector<double> r;
  for(auto& e : out)
  {
    const int ch = mtk::channel_of(e.msg);
    if(mtk::classify(e.msg) == mtk::midi_kind::pitch_bend)
      bend[ch] = bend_of(e.msg);
    else if(mtk::is_note_on(e.msg))
    {
      REQUIRE(bend.count(ch));
      r.push_back(e.msg.bytes[1] + (bend[ch] - 8192) / 8192. * range);
    }
  }
  return r;
}
}

TEST_CASE("Midi scaler: defaults keep the frequency output", "[midi_scaler]")
{
  driver d;
  d.note_on(69);
  d.note_on(60);
  d.tick();
  REQUIRE(d.fx.outputs.freq.value.size() == 2);
  CHECK(d.fx.outputs.freq.value[0] == Approx(261.6256).epsilon(1e-4));
  CHECK(d.fx.outputs.freq.value[1] == Approx(440.).epsilon(1e-4));

  // 12-TET: each note gets its own channel and a centered bend, before the note.
  const auto& o = d.out;
  REQUIRE(o.size() == 4);
  CHECK(mtk::classify(o[0].msg) == mtk::midi_kind::pitch_bend);
  CHECK(mtk::channel_of(o[0].msg) == 1);
  CHECK(bend_of(o[0].msg) == 8192);
  CHECK(mtk::is_note_on(o[1].msg));
  CHECK(mtk::channel_of(o[1].msg) == 1);
  CHECK(o[1].msg.bytes[1] == 69);
  CHECK(mtk::channel_of(o[2].msg) == 2);
  CHECK(bend_of(o[2].msg) == 8192);
  CHECK(mtk::channel_of(o[3].msg) == 2);
  CHECK(o[3].msg.bytes[1] == 60);

  d.note_off(69);
  d.raw({0x90, 60, 0}); // velocity 0 note-on ends a note
  d.tick();
  CHECK(d.total_sounding() == 0);
  CHECK(d.fx.outputs.freq.value.empty());
}

TEST_CASE("Midi scaler: pitch bend retuning lands on the scale", "[midi_scaler]")
{
  for(int range : {2, 12, 48})
  {
    for(auto scl : {pythagorean, edo19})
    {
      driver d;
      d.load_scale(scl);
      d.fx.inputs.bend_range.value = range;
      std::vector<double> expected;
      for(int key = 48; key < 84; key++)
      {
        d.note_on(key, 100, key - 48);
        expected.push_back(target(d.fx, key));
        d.tick(64);
        d.note_off(key, 1);
        d.tick(64);
      }
      const auto got = sounding_pitches(d.out, range);
      REQUIRE(got.size() == expected.size());
      for(std::size_t i = 0; i < got.size(); i++)
        CHECK(std::abs(got[i] - expected[i]) <= double(range) / 8192. + 1e-9);

      // The nearest note is used: the bend never exceeds half a semitone.
      for(auto& m : d.of_kind(mtk::midi_kind::pitch_bend))
        CHECK(std::abs(bend_of(m) - 8192) / 8192. * range <= 0.5 + 1e-3);
      CHECK(d.total_sounding() == 0);
    }
  }
}

TEST_CASE("Midi scaler: channel range, round robin, sharing and stealing", "[midi_scaler]")
{
  driver d;
  d.load_scale(pythagorean);
  d.fx.inputs.channels.value = "2-4";
  // C D E: three different offsets, on 2, 3, 4.
  d.note_on(60);
  d.note_on(62);
  d.note_on(64);
  d.tick();
  auto ons = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(ons.size() == 3);
  CHECK(mtk::channel_of(ons[0]) == 2);
  CHECK(mtk::channel_of(ons[1]) == 3);
  CHECK(mtk::channel_of(ons[2]) == 4);

  // With this 7-note scale, key 67 is the octave of 60: same offset, so it
  // shares channel 2.
  d.out.clear();
  d.note_on(67);
  d.tick();
  ons = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(ons.size() == 1);
  CHECK(mtk::channel_of(ons[0]) == 2);
  CHECK(ons[0].bytes[1] == 72);
  CHECK(d.of_kind(mtk::midi_kind::note_off).empty());

  // Key 65 has another offset: the oldest notes make room until a channel is
  // free. 60 goes first, but 67 still holds channel 2, so 62 goes too.
  d.out.clear();
  d.note_on(65);
  d.tick();
  auto offs = d.of_kind(mtk::midi_kind::note_off);
  ons = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(ons.size() == 1);
  REQUIRE(offs.size() == 2);
  CHECK(offs[0].bytes[1] == 60);
  CHECK(mtk::channel_of(offs[1]) == 3);
  CHECK(mtk::channel_of(ons[0]) == 3);
  // The stolen notes' own note-offs are swallowed.
  d.out.clear();
  d.note_off(60);
  d.note_off(62);
  d.tick();
  CHECK(d.out.empty());

  for(int k : {64, 67, 65})
    d.note_off(k);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi scaler: always send bend", "[midi_scaler]")
{
  for(bool always : {false, true})
  {
    driver d;
    d.fx.inputs.channels.value = "5";
    d.fx.inputs.always_bend.value = always;
    for(int i = 0; i < 3; i++)
    {
      d.note_on(60);
      d.tick();
      d.note_off(60);
      d.tick();
    }
    CHECK(d.of_kind(mtk::midi_kind::pitch_bend).size() == (always ? 3u : 1u));
    for(auto& m : d.of_kind(mtk::midi_kind::note_on))
      CHECK(mtk::channel_of(m) == 5);
  }
}

TEST_CASE("Midi scaler: incoming bend, controllers and pass-through", "[midi_scaler]")
{
  driver d;
  d.fx.inputs.channels.value = "1-3";
  d.note_on(60);
  d.note_on(64);
  d.tick();
  d.out.clear();

  // Full bend up with a 2 semitone range: +2 semitones on every held channel.
  d.raw({0xE0, 0x7F, 0x7F}, 3);
  d.tick();
  auto bends = d.of_kind(mtk::midi_kind::pitch_bend);
  REQUIRE(bends.size() == 2);
  CHECK(bend_of(bends[0]) == 16383);
  CHECK(bend_of(bends[1]) == 16383);
  CHECK(d.out[0].at - (d.now - 512) == 3);

  // A new note is placed so that the bend stays in range.
  d.out.clear();
  d.note_on(67);
  d.tick();
  auto on = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(on.size() == 1);
  CHECK(on[0].bytes[1] == 69);
  CHECK(std::abs(bend_of(d.of_kind(mtk::midi_kind::pitch_bend)[0]) - 8192) <= 1);

  // Sustain is copied to every channel of the range, clock passes unchanged,
  // in order.
  d.out.clear();
  d.raw({0xB0, 64, 127}, 0);
  d.raw({0xF8}, 1);
  d.tick();
  REQUIRE(d.out.size() == 4);
  for(int i = 0; i < 3; i++)
  {
    CHECK(mtk::channel_of(d.out[i].msg) == i + 1);
    CHECK(d.out[i].msg.bytes[1] == 64);
  }
  CHECK(d.out[3].msg.bytes[0] == 0xF8);

  // CC 123 ends every note and is passed on.
  d.out.clear();
  d.raw({0xB0, 123, 0});
  d.tick();
  CHECK(d.total_sounding() == 0);
  CHECK(d.of_kind(mtk::midi_kind::note_off).size() == 3);
  CHECK(d.of_kind(mtk::midi_kind::control_change).size() == 3);
}

TEST_CASE("Midi scaler: control changes and stop end held notes", "[midi_scaler]")
{
  driver d;
  d.load_scale(edo19);
  d.note_on(60);
  d.note_on(61);
  d.tick();
  CHECK(d.total_sounding() == 2);

  d.fx.inputs.bend_range.value = 48;
  d.tick();
  CHECK(d.total_sounding() == 0);
  // Their note-offs from the input have nothing left to end.
  d.note_off(60);
  d.note_off(61);
  d.tick();
  CHECK(d.total_sounding() == 0);

  d.note_on(62);
  d.note_on(63);
  d.tick();
  d.fx.inputs.channels.value = "3-9";
  d.tick();
  CHECK(d.total_sounding() == 0);

  d.note_on(62);
  d.tick();
  CHECK(d.total_sounding() == 1);
  d.fx.stop();
  d.out.clear();
  d.tick(0);
  CHECK(d.total_sounding() == 0);
  REQUIRE(d.of_kind(mtk::midi_kind::note_off).size() == 1);
}

TEST_CASE("Midi scaler: MTS sysex", "[midi_scaler]")
{
  driver d;
  d.load_scale(pythagorean);
  d.fx.inputs.output_mode.value = Scaler::output_mode_control::mts_sysex;
  d.note_on(62, 90, 5);
  d.tick();

  auto sysex = d.of_kind(mtk::midi_kind::sysex);
  REQUIRE(sysex.size() == 128 / Scaler::mts_keys_per_message);
  std::vector<int> seen;
  for(auto& m : sysex)
  {
    REQUIRE(m.size() == 8 + 4 * Scaler::mts_keys_per_message);
    CHECK(m.bytes[0] == 0xF0);
    CHECK(m.bytes[1] == 0x7F); // real-time
    CHECK(m.bytes[2] == 0x7F); // all devices
    CHECK(m.bytes[3] == 0x08); // MIDI tuning standard
    CHECK(m.bytes[4] == 0x02); // single note tuning change
    CHECK(m.bytes[5] == 0x00); // tuning program
    CHECK(m.bytes[6] == Scaler::mts_keys_per_message);
    CHECK(m.bytes.back() == 0xF7);
    for(int k = 0; k < Scaler::mts_keys_per_message; k++)
    {
      const int key = m.bytes[7 + 4 * k];
      const int semi = m.bytes[8 + 4 * k];
      const int frac = (m.bytes[9 + 4 * k] << 7) | m.bytes[10 + 4 * k];
      seen.push_back(key);
      for(int b = 0; b < 4; b++)
        CHECK(m.bytes[7 + 4 * k + b] < 0x80);
      const double t = target(d.fx, key);
      if(t >= 0. && t < 127.99)
        CHECK(semi + frac / 16384. == Approx(t).margin(1. / 16384.));
    }
  }
  REQUIRE(seen.size() == 128);
  for(int k = 0; k < 128; k++)
    CHECK(seen[k] == k);

  // The note passes unchanged, after the tuning.
  auto ons = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(ons.size() == 1);
  CHECK(ons[0].bytes[0] == 0x90);
  CHECK(ons[0].bytes[1] == 62);
  CHECK(d.out.back().msg.bytes[1] == 62);
  CHECK(d.out.back().at == 5);

  // Nothing is resent while the tuning stays the same.
  d.out.clear();
  d.tick();
  CHECK(d.out.empty());

  // A new scale or a frequency control change sends the tuning again.
  d.load_scale(edo19);
  d.tick();
  CHECK(d.of_kind(mtk::midi_kind::sysex).size() == 32);
  d.out.clear();
  d.fx.inputs.adjust.value = 10.f;
  d.tick();
  CHECK(d.of_kind(mtk::midi_kind::sysex).size() == 32);

  // Pitch bend and controllers pass unchanged in this mode.
  d.out.clear();
  d.raw({0xE3, 0, 0x50});
  d.tick();
  REQUIRE(d.out.size() == 1);
  CHECK(d.out[0].msg.bytes[0] == 0xE3);

  d.fx.stop();
  d.tick(0);
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi scaler: switching mode ends held notes", "[midi_scaler]")
{
  driver d;
  d.note_on(60);
  d.tick();
  d.fx.inputs.output_mode.value = Scaler::output_mode_control::mts_sysex;
  d.tick();
  CHECK(d.total_sounding() == 0);
  d.note_on(60);
  d.tick();
  d.fx.inputs.output_mode.value = Scaler::output_mode_control::pitch_bend;
  d.tick();
  CHECK(d.total_sounding() == 0);
}
