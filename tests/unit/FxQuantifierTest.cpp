// Midi quantify v2 (Nodes::Quantifier::v2::Node): gate, trigger on release,
// decay and release velocity; grid 0 lets notes through; every note it starts,
// it stops, also on stop and on all notes off.

#include <Fx/Quantifier_v2.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace
{
using Q = Nodes::Quantifier::v2::Node;
using enum Nodes::Quantifier::v2::NoteLength;

libremidi::message note_on(int pitch, int vel, int chan = 1)
{
  return libremidi::channel_events::note_on(chan, pitch, vel);
}
libremidi::message note_off(int pitch, int vel = 0, int chan = 1)
{
  return libremidi::channel_events::note_off(chan, pitch, vel);
}

struct Event
{
  int64_t at;
  int status;
  int pitch;
  int vel;
  bool operator==(const Event&) const = default;
};

struct Rig
{
  Q node;
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;
  std::vector<Event> events;
  int64_t pos{};
  int buffer{};

  //! 1 kHz, 120 BPM: a quarter is 500 frames.
  explicit Rig(int buffer = 100)
      : buffer{buffer}
  {
    node.prepare(
        {.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = 1000.});
    node.inputs.grid.value = 0.f;
    node.inputs.tightness.value = 1.f;
  }

  void tick(int frames, const std::vector<std::pair<int64_t, libremidi::message>>& in)
  {
    node.inputs.midi.midi_messages.clear();
    for(auto& [at, m] : in)
      if(at >= pos && at < pos + std::max(1, frames))
      {
        auto msg = m;
        msg.timestamp = at - pos;
        node.inputs.midi.midi_messages.push_back(msg);
      }
    node.outputs.midi.midi_messages.clear();
    halp::tick_flicks tk{};
    tk.frames = frames;
    tk.position_in_frames = pos;
    tk.tempo = 120.;
    tk.start_position_in_quarters = pos / 500.;
    tk.end_position_in_quarters = (pos + frames) / 500.;
    node(tk);
    for(auto& m : node.outputs.midi.midi_messages)
    {
      if(frames > 0 && (m.timestamp < 0 || m.timestamp >= frames))
        errors.push_back("timestamp " + std::to_string(m.timestamp));
      if(frames == 0 && m.timestamp != 0)
        errors.push_back("timestamp in a 0-frame tick");
      int status = m.bytes[0] & 0xF0;
      const bool note = status == 0x80 || status == 0x90;
      if(status == 0x90 && m.bytes[2] == 0)
        status = 0x80;
      if(note)
      {
        auto& n = sounding[{m.get_channel(), m.bytes[1]}];
        if(status == 0x90)
        {
          if(n++ > 0)
            errors.push_back("started twice " + std::to_string(m.bytes[1]));
        }
        else if(n == 0)
          errors.push_back("stopped while silent " + std::to_string(m.bytes[1]));
        else
          n--;
      }
      events.push_back(
          {pos + m.timestamp, note ? status : int(m.bytes[0]), m.size() > 1 ? m.bytes[1] : 0,
           m.size() > 2 ? m.bytes[2] : 0});
    }
    pos += frames;
  }

  void run(int64_t until, const std::vector<std::pair<int64_t, libremidi::message>>& in = {})
  {
    while(pos < until)
      tick(int(std::min<int64_t>(buffer, until - pos)), in);
  }

  void stop()
  {
    node.stop();
    tick(0, {});
  }

  int still_sounding() const
  {
    int t = 0;
    for(auto& [k, n] : sounding)
      t += n;
    return t;
  }

  std::vector<Event> notes() const
  {
    std::vector<Event> r;
    for(auto& e : events)
      if(e.status == 0x80 || e.status == 0x90)
        r.push_back(e);
    return r;
  }

  void check_clean()
  {
    INFO((errors.empty() ? std::string{} : errors[0]));
    CHECK(errors.empty());
  }
};
}

TEST_CASE("Midi quantify v2: the new controls default to the old behaviour", "[fx][quantifier]")
{
  Q q;
  CHECK(q.inputs.gate.value == 100);
  CHECK(!q.inputs.trigger.value);
  CHECK(q.inputs.decay.value == 0);
  CHECK(q.inputs.release_mode.value == 0);
  CHECK(q.inputs.release_velocity.value == 0);
}

TEST_CASE("Midi quantify v2: grid 0 lets the notes through", "[fx][quantifier]")
{
  Rig r;
  r.run(1000, {{17, note_on(60, 100)}, {333, note_off(60)}, {400, libremidi::channel_events::pitch_bend(1, 300)}});
  CHECK(
      r.notes()
      == std::vector<Event>{{17, 0x90, 60, 100}, {333, 0x80, 60, 0}});
  CHECK(r.events.size() == 3);
  r.check_clean();
}

TEST_CASE("Midi quantify v2: gate scales the length", "[fx][quantifier]")
{
  SECTION("Fixed duration")
  {
    for(int gate : {0, 50, 100, 150, 200})
    {
      Rig r;
      r.node.inputs.length.value = FixedDuration;
      r.node.inputs.duration.value = 0.1f;
      r.node.inputs.gate.value = gate;
      r.run(1000, {{10, note_on(60, 100)}, {20, note_off(60)}});
      REQUIRE(r.notes().size() == 2);
      CHECK(r.notes()[1].at == 10 + gate);
      r.check_clean();
    }
  }
  SECTION("Until note-off: longer")
  {
    Rig r;
    r.node.inputs.gate.value = 150;
    r.run(1000, {{100, note_on(60, 100)}, {300, note_off(60)}});
    CHECK(r.notes() == std::vector<Event>{{100, 0x90, 60, 100}, {400, 0x80, 60, 0}});
    r.check_clean();
  }
  SECTION("Until note-off: shorter ends at the release")
  {
    Rig r;
    r.node.inputs.gate.value = 50;
    r.run(1000, {{100, note_on(60, 100)}, {300, note_off(60)}});
    CHECK(r.notes() == std::vector<Event>{{100, 0x90, 60, 100}, {300, 0x80, 60, 0}});
  }
  SECTION("Until note-off, released before its grid point")
  {
    Rig r;
    r.node.inputs.grid.value = 0.5f; // a quarter: every 500 frames
    r.node.inputs.grid.sync = true;
    r.node.inputs.gate.value = 200;
    r.run(1500, {{100, note_on(60, 100)}, {150, note_off(60)}});
    CHECK(r.notes() == std::vector<Event>{{500, 0x90, 60, 100}, {600, 0x80, 60, 0}});
  }
}

TEST_CASE("Midi quantify v2: trigger plays on release", "[fx][quantifier]")
{
  SECTION("As long as the key was held")
  {
    Rig r;
    r.node.inputs.trigger.value = true;
    r.run(1000, {{100, note_on(60, 90)}, {300, note_off(60)}});
    CHECK(r.notes() == std::vector<Event>{{300, 0x90, 60, 90}, {500, 0x80, 60, 0}});
    r.check_clean();
  }
  SECTION("Fixed duration, on the grid")
  {
    Rig r;
    r.node.inputs.trigger.value = true;
    r.node.inputs.grid.value = 0.5f;
    r.node.inputs.grid.sync = true;
    r.node.inputs.length.value = FixedDuration;
    r.node.inputs.duration.value = 0.05f;
    r.run(1500, {{100, note_on(60, 90)}, {700, note_off(60)}});
    CHECK(r.notes() == std::vector<Event>{{1000, 0x90, 60, 90}, {1050, 0x80, 60, 0}});
    r.check_clean();
  }
  SECTION("Decay lowers each triggered note, and a re-trigger ends the last one")
  {
    Rig r;
    r.node.inputs.trigger.value = true;
    r.node.inputs.decay.value = 50;
    r.node.inputs.length.value = FixedDuration;
    r.node.inputs.duration.value = 0.5f;
    r.run(
        1500, {{0, note_on(60, 100)},
               {100, note_off(60)},
               {200, note_on(60, 100)},
               {300, note_off(60)},
               {1000, note_on(60, 100)},
               {1010, note_off(60)}});
    CHECK(
        r.notes()
        == std::vector<Event>{
            {100, 0x90, 60, 50},
            {300, 0x80, 60, 0},
            {300, 0x90, 60, 25},
            {800, 0x80, 60, 0},
            {1010, 0x90, 60, 50}});
    r.stop();
    CHECK(r.still_sounding() == 0);
    r.check_clean();
  }
}

TEST_CASE("Midi quantify v2: decay on a key struck again", "[fx][quantifier]")
{
  Rig r;
  r.node.inputs.decay.value = 40;
  r.run(1000, {{0, note_on(60, 100)}, {100, note_on(60, 120)}, {200, note_on(60, 120)}, {300, note_off(60)}, {400, note_on(60, 70)}, {500, note_off(60)}});
  CHECK(
      r.notes()
      == std::vector<Event>{
          {0, 0x90, 60, 100},
          {100, 0x80, 60, 0},
          {100, 0x90, 60, 60},
          {200, 0x80, 60, 0},
          {200, 0x90, 60, 36},
          {300, 0x80, 60, 0},
          {400, 0x90, 60, 70},
          {500, 0x80, 60, 0}});
  r.check_clean();
}

TEST_CASE("Midi quantify v2: release velocity", "[fx][quantifier]")
{
  SECTION("Fixed")
  {
    Rig r;
    r.node.inputs.release_velocity.value = 33;
    r.run(1000, {{0, note_on(60, 100)}, {100, note_off(60, 90)}});
    CHECK(r.notes().back() == Event{100, 0x80, 60, 33});
  }
  SECTION("From the input")
  {
    Rig r;
    r.node.inputs.release_mode.value = 1;
    r.node.inputs.release_velocity.value = 33;
    r.run(1000, {{0, note_on(60, 100)}, {100, note_off(60, 90)}});
    CHECK(r.notes().back() == Event{100, 0x80, 60, 90});
  }
  SECTION("From the input, fixed duration released early")
  {
    Rig r;
    r.node.inputs.release_mode.value = 1;
    r.node.inputs.length.value = FixedDuration;
    r.node.inputs.duration.value = 0.2f;
    r.run(1000, {{0, note_on(60, 100)}, {100, note_off(60, 77)}});
    CHECK(r.notes().back() == Event{200, 0x80, 60, 77});
  }
}

TEST_CASE("Midi quantify v2: stop and all notes off end everything", "[fx][quantifier]")
{
  Rig r;
  r.node.inputs.grid.value = 0.5f;
  r.node.inputs.grid.sync = true;
  r.run(600, {{100, note_on(60, 100)}, {200, note_on(62, 100)}, {550, note_on(64, 100)}});
  CHECK(r.still_sounding() == 2);
  r.stop();
  CHECK(r.still_sounding() == 0);
  // 64 was waiting for its grid point: it never comes
  r.run(1500, {{1100, note_off(64)}});
  CHECK(r.still_sounding() == 0);
  r.run(2000, {{1500, note_on(65, 100)}, {1600, libremidi::channel_events::control_change(1, 123, 0)}});
  CHECK(r.still_sounding() == 0);
  CHECK(r.events.back().status == 0xB0);
  r.run(3000, {{2100, note_off(65)}});
  CHECK(r.still_sounding() == 0);
  r.check_clean();
}

TEST_CASE("Midi quantify v2: random play with every control moving", "[fx][quantifier]")
{
  std::mt19937 rng(77);
  for(int round = 0; round < 30; round++)
  {
    Rig r{int(1 + rng() % 300)};
    std::vector<std::pair<int64_t, libremidi::message>> in;
    for(int i = 0; i < 80; i++)
    {
      const int64_t at = rng() % 8000;
      const int pitch = 48 + rng() % 12;
      switch(rng() % 3)
      {
        case 0:
          in.emplace_back(at, note_on(pitch, 1 + rng() % 127, 1 + rng() % 2));
          break;
        case 1:
          in.emplace_back(at, note_off(pitch, rng() % 128, 1 + rng() % 2));
          break;
        case 2:
          in.emplace_back(at, note_on(pitch, 0, 1 + rng() % 2));
          break;
      }
    }
    std::sort(in.begin(), in.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for(int64_t t = 0; t < 8000; t += 400)
    {
      r.node.inputs.grid.value = (rng() % 3) * 0.125f;
      r.node.inputs.grid.sync = rng() % 2;
      r.node.inputs.length.value = Nodes::Quantifier::v2::NoteLength(rng() % 3);
      r.node.inputs.duration.value = 0.05f + (rng() % 4) * 0.1f;
      r.node.inputs.gate.value = rng() % 201;
      r.node.inputs.trigger.value = rng() % 2;
      r.node.inputs.decay.value = rng() % 101;
      r.node.inputs.release_mode.value = rng() % 2;
      r.run(t + 400, in);
    }
    r.run(9000);
    r.stop();
    INFO("round " << round);
    r.check_clean();
    CHECK(r.still_sounding() == 0);
  }
}
