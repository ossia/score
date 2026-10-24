#include <Advanced/MidiScaler/MidiDispatcher.hpp>

#include "MidiChannelMask.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <vector>

using Dispatcher = mtk::MidiDispatcher;
using Algo = Dispatcher::algorithm_control;

namespace
{
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
  //! Off when the test sends note-offs that pass through on purpose.
  bool strict{true};

  driver()
  {
    fx.prepare(halp::setup{.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  }

  void send(libremidi::message m, int ts = 0)
  {
    m.timestamp = ts;
    fx.inputs.midi.push_back(m);
  }
  void note_on(int note, int ts = 0, int ch = 1, int vel = 100)
  {
    send(libremidi::channel_events::note_on(ch, note, vel), ts);
  }
  void note_off(int note, int ts = 0, int ch = 1)
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
      if(frames > 0)
      {
        REQUIRE(m.timestamp >= 0);
        REQUIRE(m.timestamp < frames);
      }
      else
        REQUIRE(m.timestamp == 0);
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
        if(strict || s > 0)
        {
          REQUIRE(s == 1);
          s--;
        }
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

  //! One note per tick, each released before the next.
  std::vector<int> sequence(int count)
  {
    for(int i = 0; i < count; i++)
    {
      note_on(60 + i);
      tick();
      note_off(60 + i);
      tick();
    }
    return on_channels();
  }
};
}

TEST_CASE("Midi dispatcher: round robin and ping pong", "[midi_dispatcher]")
{
  {
    driver d;
    CHECK(d.sequence(6) == std::vector<int>{1, 2, 3, 4, 1, 2});
    CHECK(d.total_sounding() == 0);
  }
  {
    driver d;
    d.fx.inputs.algorithm.value = Algo::ping_pong;
    CHECK(d.sequence(8) == std::vector<int>{1, 2, 3, 4, 3, 2, 1, 2});
  }
  {
    driver d;
    d.fx.inputs.channels.value = midi_test::channels_on({3, 5, 6});
    CHECK(d.sequence(4) == std::vector<int>{3, 5, 6, 3});
  }
}

TEST_CASE("Midi dispatcher: note-offs follow their note, surplus", "[midi_dispatcher]")
{
  driver d;
  for(int i = 0; i < 5; i++)
    d.note_on(60 + i, i);
  d.tick();
  // Four channels and no surplus channel: the fifth note ends the oldest one
  // and takes its channel.
  CHECK(d.on_channels() == std::vector<int>{1, 2, 3, 4, 1});
  {
    auto stolen = d.of_kind(mtk::midi_kind::note_off);
    REQUIRE(stolen.size() == 1);
    CHECK(mtk::channel_of(stolen[0].msg) == 1);
    CHECK(stolen[0].msg.bytes[1] == 60);
  }

  d.out.clear();
  for(int i = 0; i < 5; i++)
    d.note_off(60 + i, i);
  d.tick();
  auto offs = d.of_kind(mtk::midi_kind::note_off);
  REQUIRE(offs.size() == 4); // the stolen note's own note-off is swallowed
  const std::vector<std::pair<int, int>> expected{{2, 61}, {3, 62}, {4, 63}, {1, 64}};
  for(int i = 0; i < 4; i++)
  {
    CHECK(mtk::channel_of(offs[i].msg) == expected[i].first);
    CHECK(offs[i].msg.bytes[1] == expected[i].second);
  }

  d.out.clear();
  d.fx.inputs.surplus.value = 10;
  for(int i = 0; i < 6; i++)
    d.note_on(70 + i);
  d.tick();
  // Round robin resumes after channel 1, the last one taken.
  CHECK(d.on_channels() == std::vector<int>{2, 3, 4, 1, 10, 10});
  // Velocity 0 note-ons end notes too.
  for(int i = 0; i < 6; i++)
    d.send(libremidi::channel_events::note_on(1, 70 + i, 0));
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher: random is seeded", "[midi_dispatcher]")
{
  auto run = [](int seed) {
    driver d;
    d.fx.inputs.algorithm.value = Algo::random;
    d.fx.inputs.channels.value = midi_test::channels_on({1, 2, 3, 4, 5, 6, 7, 8});
    d.fx.inputs.seed.value = seed;
    return d.sequence(40);
  };
  const auto a = run(7);
  CHECK(a == run(7));
  CHECK(a != run(8));
  for(int c : a)
    CHECK((c >= 1 && c <= 8));
  // Every channel gets used.
  for(int c = 1; c <= 8; c++)
    CHECK(std::count(a.begin(), a.end(), c) > 0);

  // Only free channels are drawn.
  driver d;
  d.fx.inputs.algorithm.value = Algo::random;
  d.fx.inputs.seed.value = 3;
  for(int i = 0; i < 4; i++)
    d.note_on(60 + i);
  d.tick();
  auto chs = d.on_channels();
  std::sort(chs.begin(), chs.end());
  CHECK(chs == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("Midi dispatcher: chord modes", "[midi_dispatcher]")
{
  struct expect
  {
    Algo::enum_type algo;
    std::vector<int> pitches; // pitches on channels 1, 2, 3
  };
  for(auto [algo, pitches] : {expect{Algo::chord, {64, 60, 67}},
                              expect{Algo::chord_ascending, {60, 64, 67}},
                              expect{Algo::chord_descending, {67, 64, 60}}})
  {
    driver d;
    d.fx.inputs.algorithm.value = algo;
    d.note_on(64, 0);
    d.note_on(60, 10);
    d.note_on(67, 20);
    d.tick();
    // Held back for the grace period (20 ms = 960 frames after the last note).
    CHECK(d.out.empty());
    d.tick();
    auto ons = d.of_kind(mtk::midi_kind::note_on);
    REQUIRE(ons.size() == 3);
    for(int i = 0; i < 3; i++)
    {
      CHECK(ons[i].at == 980);
      CHECK(mtk::channel_of(ons[i].msg) == i + 1);
      CHECK(ons[i].msg.bytes[1] == pitches[i]);
    }
    d.note_off(60);
    d.note_off(64);
    d.note_off(67);
    d.tick();
    CHECK(d.total_sounding() == 0);
  }

  // More notes than channels: the extra ones go to the surplus channel.
  driver d;
  d.fx.inputs.algorithm.value = Algo::chord_ascending;
  d.fx.inputs.surplus.value = 16;
  d.fx.inputs.grace.value = 0.f;
  for(int p : {72, 60, 64, 67, 71, 62})
    d.note_on(p, 5);
  d.tick();
  auto ons = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(ons.size() == 6);
  const std::vector<std::pair<int, int>> want{{1, 60}, {2, 62}, {3, 64}, {4, 67}, {16, 71}, {16, 72}};
  for(int i = 0; i < 6; i++)
  {
    CHECK(ons[i].at == 5);
    CHECK(mtk::channel_of(ons[i].msg) == want[i].first);
    CHECK(ons[i].msg.bytes[1] == want[i].second);
  }

  // The next chord starts again from the first free channel.
  d.out.clear();
  for(int p : {72, 60, 64})
    d.note_off(p);
  d.note_on(50, 10);
  d.note_on(55, 10);
  d.tick();
  ons = d.of_kind(mtk::midi_kind::note_on);
  REQUIRE(ons.size() == 2);
  CHECK(mtk::channel_of(ons[0].msg) == 1);
  CHECK(mtk::channel_of(ons[1].msg) == 3);
  for(int p : {67, 71, 62, 50, 55})
    d.note_off(p);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher: a note-off completes the chord", "[midi_dispatcher]")
{
  driver d;
  d.fx.inputs.algorithm.value = Algo::chord;
  d.note_on(60, 0);
  d.note_on(64, 50);
  d.note_off(60, 100);
  d.tick();
  REQUIRE(d.out.size() == 3);
  CHECK(mtk::is_note_on(d.out[0].msg));
  CHECK(d.out[0].at == 100);
  CHECK(mtk::is_note_on(d.out[1].msg));
  CHECK(mtk::is_note_off(d.out[2].msg));
  CHECK(mtk::channel_of(d.out[2].msg) == 1);
  CHECK(d.out[2].at == 100);
  d.note_off(64);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi dispatcher: release reserve", "[midi_dispatcher]")
{
  driver d;
  d.fx.inputs.algorithm.value = Algo::chord;
  d.fx.inputs.grace.value = 0.f;
  d.fx.inputs.reserve.value = 10.f; // 480 frames
  d.note_on(60, 0);
  d.note_off(60, 100);
  d.note_on(61, 200); // channel 1 is still reserved
  d.note_off(61, 300);
  d.tick(1024);
  d.note_on(62, 0); // 1024 - 100 >= 480: channel 1 is free again
  d.note_off(62, 1);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1, 2, 1});
}

TEST_CASE("Midi dispatcher: other messages", "[midi_dispatcher]")
{
  driver d;
  d.fx.inputs.surplus.value = 9;
  d.send(libremidi::channel_events::control_change(1, 7, 100), 0);
  d.send(libremidi::channel_events::pitch_bend(2, 1000), 1);
  d.send(libremidi::message{0xF8}, 2);
  d.tick();
  REQUIRE(d.out.size() == 3);
  CHECK(d.out[0].msg.bytes[0] == 0xB0);
  CHECK(d.out[1].msg.bytes[0] == 0xE1);
  CHECK(d.out[2].msg.bytes[0] == 0xF8);
  CHECK(d.out[2].at == 2);

  d.out.clear();
  d.fx.inputs.copy_to_all.value = true;
  d.send(libremidi::channel_events::control_change(1, 64, 127), 0);
  d.tick();
  std::vector<int> chs;
  for(auto& e : d.out)
  {
    CHECK(e.msg.bytes[1] == 64);
    chs.push_back(mtk::channel_of(e.msg));
  }
  CHECK(chs == std::vector<int>{1, 2, 3, 4, 9});

  // Poly pressure follows its note.
  d.out.clear();
  d.fx.inputs.copy_to_all.value = false;
  d.note_on(60);
  d.note_on(62);
  d.send(libremidi::channel_events::poly_pressure(1, 62, 50), 3);
  d.tick();
  CHECK(mtk::channel_of(d.out.back().msg) == 2);

  // CC 123 ends the notes and is passed on.
  d.out.clear();
  d.send(libremidi::channel_events::control_change(1, 123, 0), 4);
  d.tick();
  CHECK(d.total_sounding() == 0);
  CHECK(d.of_kind(mtk::midi_kind::control_change).size() == 1);
  // The notes' own note-offs then pass through untouched on their input channel.
  d.out.clear();
  d.strict = false;
  d.note_off(60);
  d.tick();
  REQUIRE(d.out.size() == 1);
  CHECK(mtk::channel_of(d.out[0].msg) == 1);
}

TEST_CASE("Midi dispatcher: changes while notes are held, stop", "[midi_dispatcher]")
{
  driver d;
  d.note_on(60);
  d.note_on(62);
  d.tick();
  d.fx.inputs.channels.value = midi_test::channels_on({10, 11, 12});
  d.fx.inputs.algorithm.value = Algo::ping_pong;
  d.note_on(64);
  d.tick();
  d.note_off(60);
  d.note_off(62);
  d.note_off(64);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1, 2, 10});
  CHECK(d.total_sounding() == 0);

  // Channel 1 is usable again after its notes ended under another set.
  d.out.clear();
  d.fx.inputs.channels.value = midi_test::channels_on({1});
  d.note_on(65);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{1});

  // Stop: note-offs for what sounds, the chord being collected is dropped.
  d.fx.inputs.algorithm.value = Algo::chord;
  d.note_on(67, 3);
  d.tick();
  d.fx.stop();
  d.out.clear();
  d.tick(0);
  CHECK(d.total_sounding() == 0);
  CHECK(d.of_kind(mtk::midi_kind::note_off).size() == 1);
  d.tick();
  d.tick();
  CHECK(d.of_kind(mtk::midi_kind::note_on).empty());
}

TEST_CASE("Midi dispatcher: untracked note-offs do not free a channel", "[midi_dispatcher]")
{
  driver d;
  d.strict = false;
  d.fx.inputs.surplus.value = 16;
  d.note_on(60);
  d.tick();
  d.fx.stop();
  d.tick(0);
  d.out.clear();
  d.note_on(62);
  d.tick();
  REQUIRE(d.on_channels() == std::vector<int>{1});
  // The key held across the stop: its note-off passes through on channel 1.
  d.note_off(60);
  d.tick();
  d.out.clear();
  for(int p : {64, 66, 68, 70})
    d.note_on(p);
  d.tick();
  CHECK(d.on_channels() == std::vector<int>{2, 3, 4, 16});
}
