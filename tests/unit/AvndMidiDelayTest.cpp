// Midi delay (mtk::MidiDelay): echoes, feedback, pitch step, dry/wet, fixed
// length, MPE expression, the 32 remembered notes; every note it starts, it
// stops, across ticks of any size, on stop and on all notes off.

#include <Advanced/MidiScaler/MidiDelay.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace
{
libremidi::message note_on(int pitch, int vel, int chan = 1)
{
  return libremidi::channel_events::note_on(chan, pitch, vel);
}
libremidi::message note_off(int pitch, int chan = 1, int vel = 0)
{
  return libremidi::channel_events::note_off(chan, pitch, vel);
}

struct Event
{
  int64_t at;
  int status;
  int chan;
  int pitch;
  int vel;
  bool operator==(const Event&) const = default;
};

std::ostream& operator<<(std::ostream& s, const Event& e)
{
  return s << "{" << e.at << ", " << std::hex << e.status << std::dec << ", " << e.chan
           << ", " << e.pitch << ", " << e.vel << "}";
}

struct Rig
{
  mtk::MidiDelay node;
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;
  std::vector<Event> events;
  int64_t pos{};
  int buffer{};

  //! 1 kHz: Time 0.1 is 100 frames.
  explicit Rig(int buffer = 100)
      : buffer{buffer}
  {
    node.prepare(
        {.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = 1000.});
    node.inputs.time.value = 0.1f;
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
    halp::tick_musical tk{};
    tk.frames = frames;
    tk.position_in_frames = pos;
    tk.tempo = 120.;
    node(tk);
    int64_t last = 0;
    for(auto& m : node.outputs.midi.midi_messages)
    {
      if(frames > 0 && (m.timestamp < 0 || m.timestamp >= frames))
        errors.push_back("timestamp " + std::to_string(m.timestamp));
      if(frames == 0 && m.timestamp != 0)
        errors.push_back("timestamp in a 0-frame tick");
      if(m.timestamp < last)
        errors.push_back("out of order");
      last = m.timestamp;
      int status = m.bytes[0] & 0xF0;
      if(status == 0x90 && m.bytes[2] == 0)
        status = 0x80;
      if(status == 0x80 || status == 0x90)
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
          {pos + m.timestamp, status, m.get_channel(), m.size() > 1 ? m.bytes[1] : 0,
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

  std::vector<Event> ons() const
  {
    std::vector<Event> r;
    for(auto& e : events)
      if(e.status == 0x90)
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

TEST_CASE("Midi delay: echoes with feedback", "[avnd][midi][delay]")
{
  for(int buffer : {1, 7, 64, 100, 333, 4096})
  {
    INFO("buffer " << buffer);
    Rig r{buffer};
    r.node.inputs.repeats.value = 3;
    r.node.inputs.feedback.value = 50;
    r.run(1000, {{0, note_on(60, 100)}, {50, note_off(60)}});
    CHECK(
        r.events
        == std::vector<Event>{
            {0, 0x90, 1, 60, 100},
            {50, 0x80, 1, 60, 0},
            {100, 0x90, 1, 60, 50},
            {150, 0x80, 1, 60, 0},
            {200, 0x90, 1, 60, 25},
            {250, 0x80, 1, 60, 0},
            {300, 0x90, 1, 60, 13},
            {350, 0x80, 1, 60, 0}});
    CHECK(r.node.active_sources() == 0);
    r.check_clean();
  }
}

TEST_CASE("Midi delay: pitch step, floor and infinite", "[avnd][midi][delay]")
{
  SECTION("The chain ends past 127")
  {
    Rig r;
    r.node.inputs.repeats.value = 20;
    r.node.inputs.feedback.value = 100;
    r.node.inputs.pitch_step.value = 12;
    r.run(3000, {{0, note_on(60, 100)}, {50, note_off(60)}});
    std::vector<int> p;
    for(auto& e : r.ons())
      p.push_back(e.pitch);
    CHECK(p == std::vector<int>{60, 72, 84, 96, 108, 120});
    r.check_clean();
    CHECK(r.still_sounding() == 0);
  }
  SECTION("Infinite stops under the floor")
  {
    Rig r;
    r.node.inputs.infinite.value = true;
    r.node.inputs.feedback.value = 50;
    r.node.inputs.floor.value = 10;
    r.run(3000, {{0, note_on(60, 100)}, {50, note_off(60)}});
    std::vector<int> v;
    for(auto& e : r.ons())
      v.push_back(e.vel);
    CHECK(v == std::vector<int>{100, 50, 25, 13});
    CHECK(r.node.active_sources() == 0);
  }
  SECTION("Infinite at full feedback runs until stopped")
  {
    Rig r;
    r.node.inputs.infinite.value = true;
    r.node.inputs.feedback.value = 100;
    r.run(10000, {{0, note_on(60, 100)}, {150, note_off(60)}});
    CHECK(r.ons().size() == 100);
    r.stop();
    CHECK(r.still_sounding() == 0);
    r.events.clear();
    r.run(11000);
    CHECK(r.events.empty());
    r.check_clean();
  }
}

TEST_CASE("Midi delay: dry/wet", "[avnd][midi][delay]")
{
  auto velocities = [](int mix) {
    Rig r;
    r.node.inputs.repeats.value = 1;
    r.node.inputs.feedback.value = 100;
    r.node.inputs.mix.value = mix;
    r.run(1000, {{0, note_on(60, 100)}, {50, note_off(60)}});
    std::vector<int> v;
    for(auto& e : r.ons())
      v.push_back(e.vel);
    r.check_clean();
    CHECK(r.still_sounding() == 0);
    return v;
  };
  CHECK(velocities(0) == std::vector<int>{100});
  CHECK(velocities(25) == std::vector<int>{100, 50});
  CHECK(velocities(50) == std::vector<int>{100, 100});
  CHECK(velocities(75) == std::vector<int>{50, 100});
  CHECK(velocities(100) == std::vector<int>{100});
}

TEST_CASE("Midi delay: fixed length", "[avnd][midi][delay]")
{
  Rig r{64};
  r.node.inputs.repeats.value = 2;
  r.node.inputs.feedback.value = 100;
  r.node.inputs.fixed.value = true;
  r.node.inputs.length.value = 0.02f;
  SECTION("Echoes on other pitches")
  {
    r.node.inputs.pitch_step.value = 1;
    // Held for a long time: the echoes still last 20 frames
    r.run(1000, {{0, note_on(60, 100)}, {500, note_off(60)}});
    CHECK(
        r.events
        == std::vector<Event>{
            {0, 0x90, 1, 60, 100},
            {100, 0x90, 1, 61, 100},
            {120, 0x80, 1, 61, 0},
            {200, 0x90, 1, 62, 100},
            {220, 0x80, 1, 62, 0},
            {500, 0x80, 1, 60, 0}});
  }
  SECTION("Echoes on the held pitch re-attack it, it ends with the last note")
  {
    r.run(1000, {{0, note_on(60, 100)}, {500, note_off(60)}});
    CHECK(
        r.events
        == std::vector<Event>{
            {0, 0x90, 1, 60, 100},
            {100, 0x80, 1, 60, 0},
            {100, 0x90, 1, 60, 100},
            {200, 0x80, 1, 60, 0},
            {200, 0x90, 1, 60, 100},
            {500, 0x80, 1, 60, 0}});
  }
  r.check_clean();
  CHECK(r.still_sounding() == 0);
}

TEST_CASE("Midi delay: echoes longer than the delay", "[avnd][midi][delay]")
{
  Rig r{37};
  r.node.inputs.repeats.value = 3;
  r.node.inputs.feedback.value = 80;
  r.run(1000, {{0, note_on(60, 100)}, {250, note_off(60)}});
  r.check_clean();
  CHECK(r.still_sounding() == 0);
  CHECK(r.events.back().at == 550); // the last echo ends 3 delays after the key
}

TEST_CASE("Midi delay: echoes keep the channel, MPE echoes the expression", "[avnd][midi][delay]")
{
  Rig r;
  r.node.inputs.repeats.value = 2;
  r.node.inputs.feedback.value = 100;
  r.node.inputs.mpe.value = true;
  r.run(
      1000, {{0, note_on(60, 100, 3)},
             {20, libremidi::channel_events::pitch_bend(3, 9000)},
             {30, libremidi::channel_events::aftertouch(3, 40)},
             {40, libremidi::channel_events::pitch_bend(4, 100)},
             {50, note_off(60, 3)}});
  for(auto& e : r.events)
    if(e.status == 0x90 || e.status == 0x80)
      CHECK(e.chan == 3);
  std::vector<int64_t> bends;
  for(auto& e : r.events)
    if(e.status == 0xE0 && e.chan == 3)
      bends.push_back(e.at);
  CHECK(bends == std::vector<int64_t>{20, 120, 220});
  std::vector<int64_t> pressure;
  for(auto& e : r.events)
    if(e.status == 0xD0)
      pressure.push_back(e.at);
  CHECK(pressure == std::vector<int64_t>{30, 130, 230});
  // Another channel's bend is not a note's: it only passes
  CHECK(std::count_if(r.events.begin(), r.events.end(), [](auto& e) {
          return e.status == 0xE0 && e.chan == 4;
        }) == 1);
  r.check_clean();
}

TEST_CASE("Midi delay: without MPE, expression passes once", "[avnd][midi][delay]")
{
  Rig r;
  r.run(
      1000, {{0, note_on(60, 100)},
             {20, libremidi::channel_events::pitch_bend(1, 9000)},
             {20, libremidi::channel_events::control_change(1, 7, 100)},
             {50, note_off(60)}});
  std::vector<int> others;
  for(auto& e : r.events)
    if(e.status != 0x90 && e.status != 0x80)
      others.push_back(e.status);
  CHECK(others == std::vector<int>{0xE0, 0xB0});
}

TEST_CASE("Midi delay: 32 notes remembered, the oldest chain makes room", "[avnd][midi][delay]")
{
  Rig r;
  r.node.inputs.repeats.value = 8;
  r.node.inputs.feedback.value = 100;
  std::vector<std::pair<int64_t, libremidi::message>> in;
  for(int i = 0; i < 40; i++)
    in.emplace_back(i, note_on(40 + i, 100));
  for(int i = 0; i < 40; i++)
    in.emplace_back(400 + i, note_off(40 + i));
  r.run(2000, in);
  CHECK(r.node.active_sources() == 0);
  r.check_clean();
  CHECK(r.still_sounding() == 0);
  // The first 8 notes lost their chains before their first echo
  for(int i = 0; i < 40; i++)
  {
    const auto n = std::count_if(r.events.begin(), r.events.end(), [&](auto& e) {
      return e.status == 0x90 && e.pitch == 40 + i;
    });
    CHECK(n == (i < 8 ? 1 : 9));
  }
}

TEST_CASE("Midi delay: stop and all notes off", "[avnd][midi][delay]")
{
  Rig r;
  r.node.inputs.repeats.value = 10;
  r.node.inputs.feedback.value = 100;
  r.run(250, {{0, note_on(60, 100)}, {10, note_on(64, 100, 2)}, {150, note_off(60)}});
  CHECK(r.still_sounding() > 0);
  r.stop();
  CHECK(r.still_sounding() == 0);
  r.events.clear();
  r.run(2000, {{300, note_off(64, 2)}});
  CHECK(r.events.empty());

  r.run(2300, {{2000, note_on(60, 100)}, {2000, note_on(62, 100, 2)}});
  r.run(2400, {{2350, libremidi::channel_events::control_change(1, 123, 0)}});
  // Channel 1 is silent and stays so; channel 2 goes on
  CHECK(r.sounding[{1, 60}] == 0);
  const auto before = r.events.size();
  r.run(3000, {{2500, note_off(60)}, {2500, note_off(62, 2)}});
  for(std::size_t i = before; i < r.events.size(); i++)
    CHECK(r.events[i].chan == 2);
  r.stop();
  r.check_clean();
  CHECK(r.still_sounding() == 0);
}

TEST_CASE("Midi delay: random play with every control moving", "[avnd][midi][delay]")
{
  std::mt19937 rng(99);
  for(int round = 0; round < 30; round++)
  {
    Rig r{int(1 + rng() % 500)};
    std::vector<std::pair<int64_t, libremidi::message>> in;
    for(int i = 0; i < 120; i++)
    {
      const int64_t at = rng() % 8000;
      const int pitch = 48 + rng() % 12;
      const int chan = 1 + rng() % 2;
      switch(rng() % 5)
      {
        case 0:
        case 1:
          in.emplace_back(at, note_on(pitch, 1 + rng() % 127, chan));
          break;
        case 2:
          in.emplace_back(at, note_off(pitch, chan));
          break;
        case 3:
          in.emplace_back(at, note_on(pitch, 0, chan));
          break;
        case 4:
          in.emplace_back(at, libremidi::channel_events::pitch_bend(chan, rng() % 16384));
          break;
      }
    }
    std::sort(in.begin(), in.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for(int64_t t = 0; t < 8000; t += 300)
    {
      r.node.inputs.time.value = 0.01f + (rng() % 30) * 0.01f;
      r.node.inputs.repeats.value = 1 + rng() % 8;
      r.node.inputs.infinite.value = rng() % 4 == 0;
      r.node.inputs.feedback.value = rng() % 101;
      r.node.inputs.floor.value = 1 + rng() % 40;
      r.node.inputs.pitch_step.value = int(rng() % 25) - 12;
      r.node.inputs.mix.value = rng() % 101;
      r.node.inputs.fixed.value = rng() % 2;
      r.node.inputs.length.value = 0.005f + (rng() % 20) * 0.02f;
      r.node.inputs.mpe.value = rng() % 2;
      r.run(t + 300, in);
    }
    r.run(9000);
    r.stop();
    INFO("round " << round);
    r.check_clean();
    CHECK(r.still_sounding() == 0);
  }
}
