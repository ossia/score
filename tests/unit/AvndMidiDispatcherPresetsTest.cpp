// The Midi dispatcher presets of the default package load onto the object and
// do what their names say.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiDispatcher.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <vector>

using Dispatcher = mtk::MidiDispatcher;

namespace
{
const QString folder = "Midi dispatcher";
const QString uuid = "f8d568dd-1dd4-4aad-9b49-03628f874557";
constexpr int rate = 48000;

struct out_msg
{
  int64_t at{};
  libremidi::message msg;
};

struct driver
{
  Dispatcher fx;
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
  void note_on(int note, int ts = 0) { send(libremidi::channel_events::note_on(1, note, 100), ts); }
  void note_off(int note, int ts = 0) { send(libremidi::channel_events::note_off(1, note, 0), ts); }

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

  //! Ticks until `ms` milliseconds have passed.
  void run_ms(int ms)
  {
    const int64_t until = now + int64_t(ms) * rate / 1000;
    while(now < until)
      tick();
  }

  int total_sounding() const
  {
    int n = 0;
    for(auto& [k, v] : sounding)
      n += v;
    return n;
  }

  std::vector<out_msg> of_kind(mtk::midi_kind k) const
  {
    std::vector<out_msg> r;
    for(auto& e : out)
      if(mtk::classify(e.msg) == k)
        r.push_back(e);
    return r;
  }

  std::vector<int> on_channels() const
  {
    std::vector<int> r;
    for(auto& e : of_kind(mtk::midi_kind::note_on))
      r.push_back(mtk::channel_of(e.msg));
    return r;
  }

  //! Output channel of each note-on, by pitch.
  std::map<int, int> channel_by_pitch() const
  {
    std::map<int, int> r;
    for(auto& e : of_kind(mtk::midi_kind::note_on))
      r[e.msg.bytes[1]] = mtk::channel_of(e.msg);
    return r;
  }

  //! One note per tick, each released the tick after.
  void sequence(int count)
  {
    for(int i = 0; i < count; i++)
    {
      note_on(60 + i % 12);
      tick();
      note_off(60 + i % 12);
      tick();
    }
  }

  //! Plays `notes` together, waits, releases them.
  void chord(std::initializer_list<int> notes, int hold_ms = 100)
  {
    for(int n : notes)
      note_on(n);
    tick();
    run_ms(hold_ms);
    for(int n : notes)
      note_off(n);
    tick();
    run_ms(10);
  }

  std::set<int> cc_channels() const
  {
    std::set<int> r;
    for(auto& e : of_kind(mtk::midi_kind::control_change))
      r.insert(mtk::channel_of(e.msg));
    return r;
  }
};
}

TEST_CASE("Midi dispatcher presets: every preset keeps notes paired", "[presets][midi_dispatcher]")
{
  const auto presets = midi_presets::load(folder, uuid);
  REQUIRE(presets.size() >= 6);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    driver d{p.name};
    for(int i = 0; i < 30; i++)
    {
      d.note_on(48 + i % 24, 10);
      d.note_on(72 + i % 7, 12);
      d.note_on(36 + i % 5, 300);
      d.tick();
      d.run_ms(40);
      d.note_off(48 + i % 24, 20);
      d.note_off(72 + i % 7, 30);
      d.note_off(36 + i % 5, 400);
      d.tick();
      d.run_ms(40);
    }
    d.run_ms(300);
    CHECK(d.total_sounding() == 0);
    CHECK(!d.of_kind(mtk::midi_kind::note_on).empty());
  }
}

TEST_CASE("Midi dispatcher preset: Four mono synths, round robin", "[presets][midi_dispatcher]")
{
  driver d{"Four mono synths, round robin"};
  d.sequence(8);
  CHECK(d.on_channels() == std::vector<int>{1, 2, 3, 4, 1, 2, 3, 4});

  // A fifth held note finds no synth: it takes over the oldest one.
  d.out.clear();
  for(int n : {60, 62, 64, 65, 67})
    d.note_on(n);
  d.tick();
  CHECK(d.of_kind(mtk::midi_kind::note_on).size() == 5);
  CHECK(d.of_kind(mtk::midi_kind::note_off).size() == 1);

  // Mod wheel reaches the four synths.
  d.out.clear();
  d.send(libremidi::channel_events::control_change(1, 1, 90));
  d.tick();
  CHECK(d.cc_channels() == std::set<int>{1, 2, 3, 4});

  for(int n : {60, 62, 64, 65, 67})
    d.note_off(n);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher preset: Ping pong over 8 channels", "[presets][midi_dispatcher]")
{
  driver d{"Ping pong over 8 channels"};
  d.sequence(16);
  CHECK(d.on_channels() == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 2});
  // Controllers pass unchanged.
  d.out.clear();
  d.send(libremidi::channel_events::control_change(1, 1, 90));
  d.tick();
  CHECK(d.cc_channels() == std::set<int>{1});
}

TEST_CASE("Midi dispatcher preset: Poly chain, two 4-voice synths", "[presets][midi_dispatcher]")
{
  driver d{"Poly chain, two 4-voice synths"};
  for(int i = 0; i < 8; i++)
    d.note_on(60 + i);
  d.tick();
  const auto chans = d.on_channels();
  CHECK(std::set<int>(chans.begin(), chans.end()) == std::set<int>{1, 2, 3, 4, 5, 6, 7, 8});
  d.out.clear();
  d.send(libremidi::channel_events::pitch_bend(1, 12000));
  d.tick();
  std::set<int> pb;
  for(auto& e : d.of_kind(mtk::midi_kind::pitch_bend))
    pb.insert(mtk::channel_of(e.msg));
  CHECK(pb == std::set<int>{1, 2, 3, 4, 5, 6, 7, 8});
  for(int i = 0; i < 8; i++)
    d.note_off(60 + i);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher preset: Four voices, extra notes on 16", "[presets][midi_dispatcher]")
{
  driver d{"Four voices, extra notes on 16"};
  for(int n : {60, 62, 64, 65, 67, 69})
    d.note_on(n);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1, 2, 3, 4, 16, 16});
  for(int n : {60, 62, 64, 65, 67, 69})
    d.note_off(n);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher preset: String quartet, low to high on 1 to 4", "[presets][midi_dispatcher]")
{
  driver d{"String quartet, low to high on 1 to 4"};
  d.chord({67, 48, 76, 64});
  CHECK(d.channel_by_pitch() == std::map<int, int>{{48, 1}, {64, 2}, {67, 3}, {76, 4}});
  // The chord leaves once the 30 ms grace period has passed.
  const auto on = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(on.size() == 4);
  CHECK(on[0].at >= 30 * rate / 1000);
  CHECK(on[0].at <= 30 * rate / 1000 + 1);
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher preset: Top voice on channel 1", "[presets][midi_dispatcher]")
{
  driver d{"Top voice on channel 1"};
  d.chord({67, 48, 76, 64});
  CHECK(d.channel_by_pitch() == std::map<int, int>{{76, 1}, {67, 2}, {64, 3}, {48, 4}});
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher preset: Chord voices in played order", "[presets][midi_dispatcher]")
{
  driver d{"Chord voices in played order"};
  d.chord({67, 48, 76, 64});
  CHECK(d.channel_by_pitch() == std::map<int, int>{{67, 1}, {48, 2}, {76, 3}, {64, 4}});
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher preset: Random channels 1 to 6, repeatable", "[presets][midi_dispatcher]")
{
  driver a{"Random channels 1 to 6, repeatable"};
  driver b{"Random channels 1 to 6, repeatable"};
  a.sequence(30);
  b.sequence(30);
  const auto ca = a.on_channels();
  CHECK(ca == b.on_channels());
  REQUIRE(ca.size() == 30);
  const std::set<int> used(ca.begin(), ca.end());
  CHECK(used.size() >= 4);
  CHECK(*used.begin() >= 1);
  CHECK(*used.rbegin() <= 6);
}

TEST_CASE("Midi dispatcher preset: Four synths with long releases", "[presets][midi_dispatcher]")
{
  driver d{"Four synths with long releases"};
  // A short note on channel 1, then three held notes on 2, 3, 4.
  d.note_on(60);
  d.tick();
  d.note_off(60);
  d.tick();
  for(int n : {62, 64, 65})
    d.note_on(n);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1, 2, 3, 4});

  // 200 ms later, channel 1 is still in its release, but it is the one released
  // longest ago: the note goes there rather than being lost.
  d.run_ms(200);
  d.out.clear();
  d.note_on(67);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1});
  d.note_off(67);
  d.tick();

  // After 500 ms it takes notes again.
  d.run_ms(400);
  d.out.clear();
  d.note_on(69);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1});
  for(int n : {62, 64, 65, 69})
    d.note_off(n);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

namespace
{
//! The next chord is played `overlap_ms` before the previous one is released,
//! as a player does to bind chords: each voice moves to the new chord's note
//! of the same rank, on its own channel.
void legato_chords(int overlap_ms)
{
  driver d{"String quartet, low to high on 1 to 4"};
  for(int n : {48, 64, 67, 76})
    d.note_on(n);
  d.tick();
  d.run_ms(200);
  d.out.clear();
  for(int n : {50, 65, 69, 77})
    d.note_on(n);
  d.tick();
  d.run_ms(overlap_ms);
  for(int n : {48, 64, 67, 76})
    d.note_off(n);
  d.tick();
  d.run_ms(100);
  CHECK(d.channel_by_pitch() == std::map<int, int>{{50, 1}, {65, 2}, {69, 3}, {77, 4}});
  for(int n : {50, 65, 69, 77})
    d.note_off(n);
  d.tick();
  CHECK(d.total_sounding() == 0);
}
}

TEST_CASE("Midi dispatcher preset: String quartet, legato within the grace time", "[presets][midi_dispatcher]")
{
  legato_chords(0);
}

TEST_CASE(
    "Midi dispatcher preset: String quartet, legato longer than the grace time",
    "[presets][midi_dispatcher]")
{
  legato_chords(60);
}
