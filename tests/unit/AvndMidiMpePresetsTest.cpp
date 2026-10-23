// The Midi MPE presets of the default package load onto the object and do
// what their names say.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiMpe.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>

using Mpe = mtk::MidiMpe;

namespace
{
const QString folder = "Midi MPE";
const QString uuid = "89a7457e-671e-4d3e-8ab7-45607ee7d0fe";
constexpr int rate = 48000;

struct out_msg
{
  int64_t at{};
  libremidi::message msg;
};

struct driver
{
  Mpe fx;
  int64_t now{};
  std::vector<out_msg> out;
  std::map<std::pair<int, int>, int> sounding;

  explicit driver(const QString& name)
  {
    fx.prepare(halp::setup{.input_channels = 0, .output_channels = 0, .frames = 512, .rate = double(rate)});
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

  void send(libremidi::message m, int ts = 0)
  {
    m.timestamp = ts;
    fx.inputs.midi.push_back(m);
  }
  void note_on(int note, int ch = 1, int ts = 0)
  {
    send(libremidi::channel_events::note_on(ch, note, 100), ts);
  }
  void note_off(int note, int ch = 1, int ts = 0)
  {
    send(libremidi::channel_events::note_off(ch, note, 0), ts);
  }

  void tick(int frames = 512)
  {
    fx.outputs.midi.midi_messages.clear();
    fx(halp::tick{frames});
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

  std::vector<int> on_channels() const
  {
    std::vector<int> r;
    for(auto& m : of_kind(mtk::midi_kind::note_on))
      r.push_back(mtk::channel_of(m));
    return r;
  }

  //! Last pitch bend value sent per channel.
  std::map<int, int> bends() const
  {
    std::map<int, int> r;
    for(auto& m : of_kind(mtk::midi_kind::pitch_bend))
      r[mtk::channel_of(m)] = m.bytes[1] | (m.bytes[2] << 7);
    return r;
  }

  std::vector<libremidi::message> cc(int number) const
  {
    std::vector<libremidi::message> r;
    for(auto& m : of_kind(mtk::midi_kind::control_change))
      if(m.bytes[1] == number)
        r.push_back(m);
    return r;
  }
};

int bend_for(double semitones, int range)
{
  return 8192 + int(std::lround(semitones / range * 8192.));
}

//! Semitones a 14-bit bend value stands for.
double semitones_of(int bend, int range)
{
  return (bend - 8192) / 8192. * range;
}

//! Value of the RPN set on `ch` by CC 101/100/6, -1 when none.
int rpn_value(const driver& d, int ch, int rpn)
{
  int msb = -1, lsb = -1, value = -1;
  for(auto& m : d.of_kind(mtk::midi_kind::control_change))
  {
    if(mtk::channel_of(m) != ch)
      continue;
    if(m.bytes[1] == 101)
      msb = m.bytes[2];
    else if(m.bytes[1] == 100)
      lsb = m.bytes[2];
    else if(m.bytes[1] == 6 && msb == 0 && lsb == rpn)
      value = m.bytes[2];
  }
  return value;
}
}

TEST_CASE("Midi MPE presets: every preset keeps notes paired", "[presets][midi_mpe]")
{
  const auto presets = midi_presets::load(folder, uuid);
  REQUIRE(presets.size() >= 6);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    driver d{p.name};
    const bool mpe_in = d.fx.inputs.mode.value != Mpe::mode_control::midi_to_mpe;
    for(int i = 0; i < 40; i++)
    {
      const int ch = mpe_in ? 2 + i % 8 : 1;
      d.note_on(48 + i % 24, ch, 10);
      d.send(libremidi::channel_events::pitch_bend(ch, 8192 + 100 * (i % 20)), 50);
      d.send(libremidi::channel_events::aftertouch(ch, 3 * i), 60);
      d.send(libremidi::channel_events::control_change(ch, 74, 100 - i), 70);
      d.tick();
      d.note_off(48 + i % 24, ch, 20);
      d.tick();
    }
    d.tick();
    CHECK(d.total_sounding() == 0);
    CHECK(d.of_kind(mtk::midi_kind::note_on).size() == 40);
  }
}

TEST_CASE("Midi MPE preset: Keyboard to MPE synth", "[presets][midi_mpe]")
{
  driver d{"Keyboard to MPE synth"};
  d.tick();
  // MPE configuration: lower zone of 15 members, member bend range 48.
  CHECK(rpn_value(d, 1, 6) == 15);
  for(int ch = 2; ch <= 16; ch++)
    CHECK(rpn_value(d, ch, 0) == 48);

  d.out.clear();
  for(int n : {60, 64, 67})
    d.note_on(n);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{2, 3, 4});

  // Full bend up on the keyboard (2 semitones) moves the newest note only.
  d.out.clear();
  d.send(libremidi::channel_events::pitch_bend(1, 16383));
  d.tick();
  const auto b = d.bends();
  CHECK(b.size() == 1);
  CHECK(b.contains(4));
  CHECK(std::abs(b.at(4) - bend_for(2., 48)) <= 1);

  for(int n : {60, 64, 67})
    d.note_off(n);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: Keyboard bend moves the whole chord", "[presets][midi_mpe]")
{
  driver d{"Keyboard bend moves the whole chord"};
  for(int n : {60, 64, 67})
    d.note_on(n);
  d.tick();
  d.out.clear();
  d.send(libremidi::channel_events::pitch_bend(1, 0));
  d.send(libremidi::channel_events::control_change(1, 74, 20));
  d.tick();
  const auto b = d.bends();
  CHECK(b.size() == 3);
  for(int ch : {2, 3, 4})
  {
    INFO("channel " << ch);
    REQUIRE(b.contains(ch));
    CHECK(std::abs(b.at(ch) - bend_for(-2., 48)) <= 1);
  }
  std::set<int> slides;
  for(auto& m : d.cc(74))
    if(m.bytes[2] == 20)
      slides.insert(mtk::channel_of(m));
  CHECK(slides == std::set<int>{2, 3, 4});
  for(int n : {60, 64, 67})
    d.note_off(n);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: Keyboard to MPE synth, long releases", "[presets][midi_mpe]")
{
  driver d{"Keyboard to MPE synth, long releases"};
  // A fast trill: every note gets a member channel and none is lost, even
  // when the members are all still reserved for their releases.
  for(int i = 0; i < 20; i++)
  {
    d.note_on(i % 2 ? 62 : 60);
    d.tick();
    d.note_off(i % 2 ? 62 : 60);
    d.tick();
  }
  const auto chans = d.on_channels();
  REQUIRE(chans.size() == 20);
  for(int i = 0; i < 15; i++)
    CHECK(chans[i] == 2 + i);
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: MPE controller to a mono-timbral synth", "[presets][midi_mpe]")
{
  driver d{"MPE controller to a mono-timbral synth"};
  d.note_on(60, 2);
  d.note_on(64, 3);
  // One semitone up on the newest note's member channel (MPE range 48).
  d.send(libremidi::channel_events::pitch_bend(3, bend_for(1., 48)));
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1, 1});
  const auto b = d.bends();
  CHECK(b.size() == 1);
  REQUIRE(b.contains(1));
  CHECK(std::abs(b.at(1) - bend_for(semitones_of(bend_for(1., 48), 48), 2)) <= 1);
  d.note_off(60, 2);
  d.note_off(64, 3);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: MPE controller to a synth with bend 12", "[presets][midi_mpe]")
{
  driver d{"MPE controller to a synth with bend 12"};
  d.note_on(60, 5);
  d.send(libremidi::channel_events::pitch_bend(5, bend_for(-3., 48)));
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1});
  const auto b = d.bends();
  REQUIRE(b.contains(1));
  CHECK(std::abs(b.at(1) - bend_for(semitones_of(bend_for(-3., 48), 48), 12)) <= 1);
  d.note_off(60, 5);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: Soft pressure curve", "[presets][midi_mpe]")
{
  driver d{"Soft pressure curve"};
  d.note_on(60, 2);
  d.send(libremidi::channel_events::aftertouch(2, 64));
  d.tick();
  const auto p = d.of_kind(mtk::midi_kind::channel_pressure);
  REQUIRE(p.size() == 1);
  CHECK(mtk::channel_of(p[0]) == 2);
  CHECK(p[0].bytes[1] == std::lround(127. * std::pow(64. / 127., 2.)));
  CHECK(d.on_channels() == std::vector<int>{2});
  d.note_off(60, 2);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: Aggressive slide", "[presets][midi_mpe]")
{
  driver d{"Aggressive slide"};
  d.note_on(60, 3);
  d.send(libremidi::channel_events::control_change(3, 74, 32));
  d.tick();
  const auto s = d.cc(74);
  REQUIRE(s.size() == 1);
  CHECK(mtk::channel_of(s[0]) == 3);
  CHECK(s[0].bytes[2] == std::lround(127. * std::sqrt(32. / 127.)));
  d.note_off(60, 3);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: Half the pitch bend", "[presets][midi_mpe]")
{
  driver d{"Half the pitch bend"};
  d.note_on(60, 4);
  d.send(libremidi::channel_events::pitch_bend(4, bend_for(4., 48)));
  d.tick();
  const auto b = d.bends();
  REQUIRE(b.contains(4));
  CHECK(std::abs(b.at(4) - bend_for(2., 48)) <= 1);
  d.note_off(60, 4);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: Analog drift", "[presets][midi_mpe]")
{
  driver d{"Analog drift"};
  std::set<int> values;
  for(int i = 0; i < 15; i++)
  {
    d.out.clear();
    d.note_on(60 + i);
    d.tick();
    const auto chans = d.on_channels();
    REQUIRE(chans.size() == 1);
    const auto b = d.bends();
    REQUIRE(b.contains(chans[0]));
    // Within 0.1 semitone (10 cents) of the note.
    CHECK(std::abs(b.at(chans[0]) - 8192) <= 8192. * 0.1 / 48 + 1);
    values.insert(b.at(chans[0]));
  }
  CHECK(values.size() >= 5);
  for(int i = 0; i < 15; i++)
    d.note_off(60 + i);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE preset: Random timbre per note", "[presets][midi_mpe]")
{
  driver d{"Random timbre per note"};
  std::set<int> values;
  for(int i = 0; i < 10; i++)
  {
    d.out.clear();
    d.note_on(60 + i);
    d.tick();
    const auto chans = d.on_channels();
    REQUIRE(chans.size() == 1);
    int slide = -1;
    for(auto& m : d.cc(74))
      if(mtk::channel_of(m) == chans[0])
        slide = m.bytes[2];
    // Around the centre, at most 0.3 of the full range away.
    CHECK(std::abs(slide - 64) <= int(0.3 * 127) + 1);
    values.insert(slide);
    d.note_off(60 + i);
    d.tick();
  }
  CHECK(values.size() >= 4);
  CHECK(d.total_sounding() == 0);
}
