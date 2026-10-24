// Arpeggiator v2 (Nodes::Arpeggiator::v2::Node): with its new controls at
// their defaults it plays what the old arpeggiator plays; the new patterns,
// gate, swing, accents, decay, retrigger, latch and pressure; every note it
// starts, it stops, also on stop and on all notes off.

#include <Fx/Arpeggiator_v2.hpp>

#include "AllocCounter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <map>
#include <new>
#include <random>
#include <string>
#include <tuple>
#include <vector>

namespace
{
using Arp = Nodes::Arpeggiator::v2::Node;
using Old = Nodes::Arpeggiator::Node;

libremidi::message note_on(int pitch, int vel, int chan = 1)
{
  return libremidi::channel_events::note_on(chan, pitch, vel);
}
libremidi::message note_off(int pitch, int chan = 1)
{
  return libremidi::channel_events::note_off(chan, pitch, 0);
}

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
    auto& n = sounding[{chan, pitch}];
    if(on)
    {
      if(n > 0)
        errors.push_back("started twice: " + std::to_string(pitch));
      n++;
    }
    else
    {
      if(n == 0)
        errors.push_back("stopped while silent: " + std::to_string(pitch));
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

struct Event
{
  int64_t at;
  int status; // 0x90 or 0x80 for notes, the status byte otherwise
  int pitch;
  int vel;
  bool operator==(const Event&) const = default;
};

Event event_of(const libremidi::message& m, int64_t at)
{
  int status = m.bytes[0];
  if((status & 0xF0) == 0x90 && m.bytes[2] == 0)
    status = 0x80;
  else if((status & 0xF0) == 0x90 || (status & 0xF0) == 0x80)
    status &= 0xF0;
  return {at, status, m.size() > 1 ? m.bytes[1] : 0, m.size() > 2 ? m.bytes[2] : 0};
}

//! 1 kHz, 120 BPM: a quarter is 500 frames, a sixteenth 125.
template <typename Node>
struct Rig
{
  Node node;
  Listener out;
  std::vector<Event> events;
  std::vector<std::string> errors;
  int64_t pos{};
  int buffer{100};
  int sig_num{4};
  int sig_denom{4};

  explicit Rig(int buffer = 100)
      : buffer{buffer}
  {
    if constexpr(requires { node.prepare(halp::setup{}); })
      node.prepare(
          {.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = 1000.});
    if constexpr(requires { node.inputs.rate; })
    {
      node.inputs.rate.value = 0.125f; // a sixteenth at 120 BPM
      node.inputs.rate.sync = true;
    }
    else
      node.inputs.quantification.value = 16;
  }

  void tick(int frames, const std::vector<std::pair<int64_t, libremidi::message>>& in = {})
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
    tk.start_position_in_quarters = pos / 500.;
    tk.end_position_in_quarters = (pos + frames) / 500.;
    tk.tempo = 120.;
    tk.signature = {sig_num, sig_denom};
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
      out.feed(m);
      events.push_back(event_of(m, pos + m.timestamp));
    }
    pos += frames;
  }

  //! Runs until `until`, the inputs at their absolute frames.
  void run(int64_t until, const std::vector<std::pair<int64_t, libremidi::message>>& in = {})
  {
    while(pos < until)
      tick(int(std::min<int64_t>(buffer, until - pos)), in);
  }

  void stop()
  {
    node.stop();
    tick(0);
  }

  std::vector<Event> ons() const
  {
    std::vector<Event> r;
    for(auto& e : events)
      if(e.status == 0x90)
        r.push_back(e);
    return r;
  }
  std::vector<int> on_pitches() const
  {
    std::vector<int> r;
    for(auto& e : ons())
      r.push_back(e.pitch);
    return r;
  }

  void check_clean()
  {
    INFO((errors.empty() ? std::string{} : errors[0]));
    CHECK(errors.empty());
    INFO((out.errors.empty() ? std::string{} : out.errors[0]));
    CHECK(out.errors.empty());
  }
};

std::vector<int> pattern(int mode, std::vector<int> notes)
{
  Nodes::Arpeggiator::Engine e;
  for(int n : notes)
    e.notes.insert({(unsigned char)n, 100});
  e.previous_arpeggio = mode;
  e.previous_repeat = 1;
  e.previous_octave = 1;
  e.update();
  std::vector<int> r;
  for(auto& c : e.arpeggio)
  {
    REQUIRE(c.size() == 1);
    r.push_back(c[0].first);
  }
  return r;
}
}

TEST_CASE("Arpeggiator v2: the old patterns keep their names and indices", "[fx][arpeggiator]")
{
  Nodes::Arpeggiator::Arpeggios::range old_r;
  Nodes::Arpeggiator::v2::Arpeggios::range new_r;
  for(int i = 0; i < 6; i++)
  {
    CHECK(std::string_view{old_r.values[i].first} == std::string_view{new_r.values[i].first});
    CHECK(old_r.values[i].second == new_r.values[i].second);
  }
  CHECK(new_r.init == 0);
}

TEST_CASE("Arpeggiator v2: at its defaults it plays what the old one plays", "[fx][arpeggiator]")
{
  for(int mode : {0, 1, 2, 3, 4})
    for(int buffer : {1, 64, 100, 333})
    {
      INFO("mode " << mode << " buffer " << buffer);
      Rig<Arp> n{buffer};
      // The old one applies a tick's input before its steps: with one-frame
      // ticks, that is in time order, as the new one plays at any buffer size
      Rig<Old> o{1};
      n.node.inputs.arpeggios.value = mode;
      o.node.inputs.arpeggios.value = mode;
      n.node.inputs.octave.value = o.node.inputs.octave.value = 2;
      n.node.inputs.repeat.value = o.node.inputs.repeat.value = 2;
      const std::vector<std::pair<int64_t, libremidi::message>> in{
          {10, note_on(60, 100)}, {40, note_on(64, 90)},  {300, note_on(67, 80)},
          {900, note_off(64)},    {1300, note_off(60)},   {1450, note_off(67)},
          {1700, note_on(62, 70)}, {2200, note_on(62, 0)}};
      n.run(2600, in);
      o.run(2600, in);
      CHECK(n.events == o.events);
      CHECK(!n.events.empty());
      n.check_clean();
      CHECK(n.out.still_sounding() == 0);
    }
}

TEST_CASE("Arpeggiator v2: new patterns", "[fx][arpeggiator]")
{
  const std::vector<int> c{60, 64, 67, 71};
  CHECK(pattern(6, c) == std::vector<int>{60, 64, 67, 71, 67, 64});
  CHECK(pattern(7, c) == std::vector<int>{71, 67, 64, 60, 64, 67});
  CHECK(pattern(8, c) == std::vector<int>{60, 71, 64, 67});
  CHECK(pattern(9, c) == std::vector<int>{67, 64, 71, 60});
  CHECK(pattern(10, c) == std::vector<int>{60, 64, 60, 67, 60, 71});
  CHECK(pattern(11, c) == std::vector<int>{71, 60, 71, 64, 71, 67});
  CHECK(pattern(8, {60, 64, 67}) == std::vector<int>{60, 67, 64});
  for(int mode = 6; mode <= 11; mode++)
  {
    CHECK(pattern(mode, {60}) == std::vector<int>{60});
    CHECK(pattern(mode, {}).empty());
  }
  CHECK(pattern(6, {60, 64}) == std::vector<int>{60, 64});
  CHECK(pattern(7, {60, 64}) == std::vector<int>{64, 60});
}

TEST_CASE("Arpeggiator v2: As played follows the order of the keys", "[fx][arpeggiator]")
{
  Rig<Arp> r;
  r.node.inputs.arpeggios.value = 12;
  r.run(
      1000, {{0, note_on(67, 100)},
             {0, note_on(60, 100)},
             {0, note_on(71, 100)},
             {300, note_off(60)},
             {300, note_on(62, 100)}});
  // Steps every 125 frames: 67 60 71, then without 60 and with 62 at the end
  auto p = r.on_pitches();
  REQUIRE(p.size() >= 6);
  CHECK(std::vector<int>(p.begin(), p.begin() + 3) == std::vector<int>{67, 60, 71});
  CHECK(std::vector<int>(p.begin() + 3, p.begin() + 6) == std::vector<int>{67, 71, 62});
  r.run(1100, {{1000, note_off(67)}, {1000, note_off(71)}, {1000, note_off(62)}});
  CHECK(r.out.still_sounding() == 0);
  r.check_clean();
}

TEST_CASE("Arpeggiator v2: gate", "[fx][arpeggiator]")
{
  SECTION("Shorter than the step")
  {
    Rig<Arp> r{64};
    r.node.inputs.gate.value = 0.5f;
    r.run(1000, {{0, note_on(60, 100)}, {0, note_on(64, 100)}});
    // Each note ends half of 125 frames after it starts
    std::map<int, int64_t> started;
    int checked = 0;
    for(auto& e : r.events)
    {
      if(e.status == 0x90)
        started[e.pitch] = e.at;
      else if(e.status == 0x80)
      {
        CHECK(e.at - started[e.pitch] >= 62);
        CHECK(e.at - started[e.pitch] <= 63);
        checked++;
      }
    }
    CHECK(checked >= 6);
    r.run(1100, {{1000, note_off(60)}, {1000, note_off(64)}});
    r.check_clean();
    CHECK(r.out.still_sounding() == 0);
  }
  SECTION("Longer than the step: the notes overlap")
  {
    Rig<Arp> r{100};
    r.node.inputs.gate.value = 1.8f;
    r.run(1000, {{0, note_on(60, 100)}, {0, note_on(64, 100)}, {0, note_on(67, 100)}});
    // 60 starts at 0 and ends at 225, after 64 started at 125
    bool overlapped = false;
    for(auto& e : r.events)
      if(e.status == 0x80 && e.pitch == 60 && e.at == 225)
        overlapped = true;
    CHECK(overlapped);
    r.run(1100, {{1000, note_off(60)}, {1000, note_off(64)}, {1000, note_off(67)}});
    r.check_clean();
    CHECK(r.out.still_sounding() == 0);
  }
  SECTION("Longer than the step on a single key: it ends before it starts again")
  {
    Rig<Arp> r{100};
    r.node.inputs.gate.value = 2.f;
    r.run(800, {{0, note_on(60, 100)}});
    CHECK(r.ons().size() >= 6);
    r.check_clean();
    r.stop();
    CHECK(r.out.still_sounding() == 0);
    r.check_clean();
  }
}

TEST_CASE("Arpeggiator v2: swing delays every second step", "[fx][arpeggiator]")
{
  for(int buffer : {1, 50, 100, 333})
  {
    Rig<Arp> r{buffer};
    r.node.inputs.swing.value = 0.5f;
    r.run(1000, {{0, note_on(60, 100)}, {0, note_on(64, 100)}});
    std::vector<int64_t> at;
    for(auto& e : r.ons())
      at.push_back(e.at);
    // Second of each pair late by a third of a step: 125 / 3
    REQUIRE(at.size() >= 6);
    CHECK(std::vector<int64_t>(at.begin(), at.begin() + 6)
          == std::vector<int64_t>{0, 167, 250, 417, 500, 667});
    r.run(1100, {{1000, note_off(60)}, {1000, note_off(64)}});
    r.check_clean();
    CHECK(r.out.still_sounding() == 0);
  }
}

TEST_CASE("Arpeggiator v2: the swing pairs and the steps follow the bars", "[fx][arpeggiator]")
{
  SECTION("7/8, eighths: the downbeat of every bar is straight")
  {
    for(int buffer : {1, 100, 333})
    {
      INFO("buffer " << buffer);
      Rig<Arp> r{buffer};
      r.sig_num = 7;
      r.sig_denom = 8;
      r.node.inputs.rate.value = 0.25f; // an eighth: 250 frames, 7 to a bar
      r.node.inputs.swing.value = 0.5f;
      r.run(2300, {{0, note_on(60, 100)}});
      std::vector<int64_t> at;
      for(auto& e : r.ons())
        at.push_back(e.at);
      // Second of each pair of the bar late by a third of a step: 250 / 3.
      // The seventh eighth is the first of a pair: the bar line, at 1750,
      // starts a new one.
      CHECK(
          at
          == std::vector<int64_t>{0, 333, 500, 833, 1000, 1333, 1500, 1750, 2083, 2250});
      r.stop();
      r.check_clean();
      CHECK(r.out.still_sounding() == 0);
    }
  }
  SECTION("3/4, whole-note steps: a step is a bar, the gate a share of it")
  {
    Rig<Arp> r{100};
    r.sig_num = 3;
    r.sig_denom = 4;
    r.node.inputs.rate.value = 2.f; // a whole note: one bar of 3/4, 1500 frames
    r.node.inputs.gate.value = 0.5f;
    r.run(3200, {{0, note_on(60, 100)}, {0, note_on(64, 100)}});
    std::vector<Event> notes;
    for(auto& e : r.events)
      if(e.status == 0x90 || e.status == 0x80)
        notes.push_back(e);
    CHECK(
        notes
        == std::vector<Event>{
            {0, 0x90, 60, 100},
            {750, 0x80, 60, 0},
            {1500, 0x90, 64, 100},
            {2250, 0x80, 64, 0},
            {3000, 0x90, 60, 100}});
    r.stop();
    r.check_clean();
    CHECK(r.out.still_sounding() == 0);
  }
}

TEST_CASE("Arpeggiator v2: the input changes the steps from its own frame on", "[fx][arpeggiator]")
{
  SECTION("A key pressed late in a buffer plays from the next step after it")
  {
    Rig<Arp> r{1000};
    r.run(1000, {{400, note_on(64, 100)}});
    REQUIRE(!r.ons().empty());
    CHECK(r.ons()[0] == Event{500, 0x90, 64, 100});
    r.run(1100, {{1050, note_off(64)}});
    r.check_clean();
    CHECK(r.out.still_sounding() == 0);
  }
  SECTION("A key released late in a buffer plays in the steps before it")
  {
    Rig<Arp> r{1000};
    r.run(1000, {{0, note_on(60, 100)}, {400, note_off(60)}});
    CHECK(
        r.events
        == std::vector<Event>{
            {0, 0x90, 60, 100},
            {125, 0x80, 60, 0},
            {125, 0x90, 60, 100},
            {250, 0x80, 60, 0},
            {250, 0x90, 60, 100},
            {375, 0x80, 60, 0},
            {375, 0x90, 60, 100},
            {400, 0x80, 60, 0}});
    r.check_clean();
    CHECK(r.out.still_sounding() == 0);
  }
}

TEST_CASE("Arpeggiator v2: accents and velocity decay", "[fx][arpeggiator]")
{
  SECTION("Accent")
  {
    Rig<Arp> r;
    r.node.inputs.accent_every.value = 3;
    r.node.inputs.accent_velocity.value = 120;
    r.run(1000, {{0, note_on(60, 80)}});
    std::vector<int> v;
    for(auto& e : r.ons())
      v.push_back(e.vel);
    REQUIRE(v.size() >= 6);
    CHECK(std::vector<int>(v.begin(), v.begin() + 6) == std::vector<int>{120, 80, 80, 120, 80, 80});
  }
  SECTION("Decay")
  {
    Rig<Arp> r;
    r.node.inputs.decay.value = 0.5f;
    r.node.inputs.decay_target.value = 20;
    r.run(1000, {{0, note_on(60, 100)}});
    std::vector<int> v;
    for(auto& e : r.ons())
      v.push_back(e.vel);
    REQUIRE(v.size() >= 5);
    CHECK(std::vector<int>(v.begin(), v.begin() + 5) == std::vector<int>{100, 60, 40, 30, 25});
    // A new chord starts from the key's velocity again
    r.run(1100, {{1000, note_off(60)}});
    const auto before = r.ons().size();
    r.run(1600, {{1100, note_on(62, 100)}});
    REQUIRE(r.ons().size() > before);
    CHECK(r.ons()[before].vel == 100);
    r.check_clean();
  }
}

TEST_CASE("Arpeggiator v2: retrigger", "[fx][arpeggiator]")
{
  SECTION("On beat")
  {
    Rig<Arp> r;
    r.node.inputs.retrigger.value = Nodes::Arpeggiator::v2::RetriggerOnBeat;
    r.run(1000, {{0, note_on(60, 100)}, {0, note_on(64, 100)}, {0, note_on(67, 100)}});
    auto p = r.on_pitches();
    REQUIRE(p.size() >= 8);
    CHECK(std::vector<int>(p.begin(), p.begin() + 8)
          == std::vector<int>{60, 64, 67, 60, 60, 64, 67, 60});
  }
  SECTION("Off")
  {
    Rig<Arp> r;
    r.run(1000, {{0, note_on(60, 100)}, {0, note_on(64, 100)}, {0, note_on(67, 100)}});
    auto p = r.on_pitches();
    REQUIRE(p.size() >= 8);
    CHECK(std::vector<int>(p.begin(), p.begin() + 8)
          == std::vector<int>{60, 64, 67, 60, 64, 67, 60, 64});
  }
  SECTION("On a new key")
  {
    Rig<Arp> r;
    r.node.inputs.retrigger.value = Nodes::Arpeggiator::v2::RetriggerOnNote;
    // Steps at 0 125 250: 60 64 60; the new key at 310 restarts from the bottom
    r.run(600, {{0, note_on(60, 100)}, {0, note_on(64, 100)}, {310, note_on(67, 100)}});
    auto p = r.on_pitches();
    REQUIRE(p.size() >= 5);
    CHECK(std::vector<int>(p.begin(), p.begin() + 5) == std::vector<int>{60, 64, 60, 60, 64});
  }
}

TEST_CASE("Arpeggiator v2: latch", "[fx][arpeggiator]")
{
  SECTION("Toggle: notes stay until a new chord")
  {
    Rig<Arp> r;
    r.node.inputs.latch.value = Nodes::Arpeggiator::v2::LatchToggle;
    r.run(500, {{0, note_on(60, 100)}, {0, note_on(64, 100)}, {100, note_off(60)}, {110, note_off(64)}});
    r.events.clear();
    r.run(1000);
    auto p = r.on_pitches();
    CHECK(p.size() == 4);
    CHECK(std::count(p.begin(), p.end(), 60) == 2);
    r.events.clear();
    r.run(1500, {{1000, note_on(67, 100)}});
    p = r.on_pitches();
    CHECK(p == std::vector<int>(p.size(), 67));
    // Latch off: what is not held stops; 67 is held
    r.node.inputs.latch.value = Nodes::Arpeggiator::v2::NoLatch;
    r.events.clear();
    r.run(2000, {{1700, note_off(67)}});
    CHECK(r.out.still_sounding() == 0);
    r.events.clear();
    r.run(2500);
    CHECK(r.ons().empty());
    r.check_clean();
  }
  SECTION("Momentary: notes stay while Hold is on")
  {
    Rig<Arp> r;
    r.node.inputs.latch.value = Nodes::Arpeggiator::v2::LatchMomentary;
    r.node.inputs.hold.value = true;
    r.run(500, {{0, note_on(60, 100)}, {100, note_off(60)}, {200, note_on(64, 100)}, {300, note_off(64)}});
    r.events.clear();
    r.run(1000);
    auto p = r.on_pitches();
    CHECK(std::count(p.begin(), p.end(), 60) == 2);
    CHECK(std::count(p.begin(), p.end(), 64) == 2);
    r.node.inputs.hold.value = false;
    r.run(1100);
    CHECK(r.out.still_sounding() == 0);
    r.events.clear();
    r.run(1500);
    CHECK(r.ons().empty());
    r.check_clean();
  }
}

TEST_CASE("Arpeggiator v2: pressure sets the velocity of the next steps", "[fx][arpeggiator]")
{
  SECTION("Channel")
  {
    Rig<Arp> r;
    r.node.inputs.arpeggios.value = 4; // Chord: a note repeat
    r.node.inputs.pressure.value = Nodes::Arpeggiator::v2::ChannelPressure;
    r.run(
        1000, {{0, note_on(60, 100)},
               {0, note_on(64, 100)},
               {300, libremidi::channel_events::aftertouch(1, 30)}});
    for(auto& e : r.ons())
      CHECK(e.vel == (e.at < 300 ? 100 : 30));
    // Consumed: the pressure does not reach the output
    for(auto& e : r.events)
      CHECK(e.status != 0xD0);
    r.run(1100, {{1000, note_off(60)}, {1000, note_off(64)}});
    r.check_clean();
    CHECK(r.out.still_sounding() == 0);
  }
  SECTION("Poly")
  {
    Rig<Arp> r;
    r.node.inputs.arpeggios.value = 4;
    r.node.inputs.pressure.value = Nodes::Arpeggiator::v2::PolyPressure;
    r.run(
        1000, {{0, note_on(60, 100)},
               {0, note_on(64, 100)},
               {300, libremidi::channel_events::poly_pressure(1, 64, 45)}});
    for(auto& e : r.ons())
      CHECK(e.vel == (e.at > 300 && e.pitch == 64 ? 45 : 100));
  }
  SECTION("Off: pressure passes through, velocities stay")
  {
    Rig<Arp> r;
    r.node.inputs.arpeggios.value = 4;
    r.run(
        1000, {{0, note_on(60, 100)}, {300, libremidi::channel_events::aftertouch(1, 30)}});
    for(auto& e : r.ons())
      CHECK(e.vel == 100);
    CHECK(std::count_if(r.events.begin(), r.events.end(), [](auto& e) {
            return e.status == 0xD0 && e.at == 300;
          }) == 1);
  }
}

TEST_CASE("Arpeggiator v2: other messages pass through in order", "[fx][arpeggiator]")
{
  Rig<Arp> r;
  r.run(
      300, {{5, libremidi::channel_events::pitch_bend(1, 1000)},
            {5, libremidi::channel_events::control_change(1, 1, 64)},
            {7, libremidi::channel_events::program_change(1, 3)}});
  REQUIRE(r.events.size() == 3);
  CHECK(r.events[0].status == 0xE0);
  CHECK(r.events[0].at == 5);
  CHECK(r.events[1].status == 0xB0);
  CHECK(r.events[2].status == 0xC0);
  CHECK(r.events[2].at == 7);
}

TEST_CASE("Arpeggiator v2: stop and all notes off end everything", "[fx][arpeggiator]")
{
  for(int gate : {50, 100, 200})
  {
    INFO("gate " << gate);
    Rig<Arp> r;
    r.node.inputs.gate.value = float(gate) / 100.f;
    r.node.inputs.swing.value = 0.6f;
    r.node.inputs.latch.value = Nodes::Arpeggiator::v2::LatchToggle;
    r.run(450, {{0, note_on(60, 100)}, {0, note_on(64, 100)}, {50, note_off(60)}, {60, note_off(64)}});
    r.stop();
    CHECK(r.out.still_sounding() == 0);
    // Nothing comes back after the stop: the latched notes are forgotten
    r.events.clear();
    r.run(1000);
    CHECK(r.ons().empty());

    r.run(1450, {{1000, note_on(62, 100)}, {1001, note_on(65, 100)}});
    CHECK(r.out.still_sounding() > 0);
    r.run(1500, {{1460, libremidi::channel_events::control_change(1, 123, 0)}});
    CHECK(r.out.still_sounding() == 0);
    CHECK(std::count_if(r.events.begin(), r.events.end(), [](auto& e) {
            return e.status == 0xB0 && e.pitch == 123;
          }) == 1);
    r.events.clear();
    r.run(2000);
    CHECK(r.ons().empty());
    r.check_clean();
  }
}

TEST_CASE("Arpeggiator v2: a seeded random pattern repeats after a stop", "[fx][arpeggiator]")
{
  auto take = [](Rig<Arp>& r, int64_t from) {
    r.run(from + 2000, {{from, note_on(60, 100)}, {from, note_on(62, 100)},
                        {from, note_on(64, 100)}, {from, note_on(65, 100)},
                        {from, note_on(67, 100)}});
    std::vector<int> p;
    for(auto& e : r.ons())
      if(e.at >= from)
        p.push_back(e.pitch);
    r.stop();
    return p;
  };
  Rig<Arp> a, b;
  a.node.inputs.arpeggios.value = b.node.inputs.arpeggios.value = 5;
  a.node.inputs.seed.value = b.node.inputs.seed.value = 42;
  const auto first = take(a, 0);
  CHECK(first == take(b, 0));
  CHECK(first == take(a, a.pos));
  CHECK(first.size() == 16);
  a.check_clean();
}

TEST_CASE("Arpeggiator v2: random play with every control moving", "[fx][arpeggiator]")
{
  std::mt19937 rng(1234);
  for(int round = 0; round < 20; round++)
  {
    Rig<Arp> r{int(1 + rng() % 300)};
    std::vector<std::pair<int64_t, libremidi::message>> in;
    for(int i = 0; i < 60; i++)
    {
      const int64_t at = rng() % 8000;
      const int pitch = 48 + rng() % 24;
      switch(rng() % 6)
      {
        case 0:
        case 1:
          in.emplace_back(at, note_on(pitch, 1 + rng() % 127));
          break;
        case 2:
        case 3:
          in.emplace_back(at, note_off(pitch));
          break;
        case 4:
          in.emplace_back(at, note_on(pitch, 0));
          break;
        case 5:
          in.emplace_back(at, libremidi::channel_events::aftertouch(1, rng() % 128));
          break;
      }
    }
    std::sort(in.begin(), in.end(), [](auto& a, auto& b) { return a.first < b.first; });
    r.node.inputs.pressure.value = Nodes::Arpeggiator::v2::AnyPressure;
    for(int64_t t = 0; t < 8000; t += 500)
    {
      r.node.inputs.arpeggios.value = rng() % 13;
      r.node.inputs.gate.value = float(1 + rng() % 200) / 100.f;
      r.node.inputs.swing.value = float(rng() % 101) / 100.f;
      r.node.inputs.octave.value = 1 + rng() % 3;
      r.node.inputs.repeat.value = 1 + rng() % 3;
      r.node.inputs.latch.value = rng() % 3;
      r.node.inputs.hold.value = rng() % 2;
      r.node.inputs.retrigger.value = rng() % 3;
      r.node.inputs.rate.value = (rng() % 2) ? 0.125f : 0.04f;
      r.run(t + 500, in);
    }
    r.stop();
    INFO("round " << round);
    r.check_clean();
    CHECK(r.out.still_sounding() == 0);
  }
}

TEST_CASE(
    "Arpeggiator engine: the longest arpeggios are built without allocating",
    "[fx][arpeggiator]")
{
  using Engine = Nodes::Arpeggiator::Engine;
  Engine e;
  for(int p = 0; p < 128; p++)
    e.notes.insert({(unsigned char)p, 100});
  // Forward, Backward, F->B, B->F, Random, Up/Down, Down/Up, Converge,
  // Diverge, Low Up, High Up
  for(int mode : {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11})
    for(int octave_mode : {0, 1, 2})
    {
      INFO("mode " << mode << " octave mode " << octave_mode);
      e.previous_arpeggio = mode;
      e.previous_octave = Engine::max_octave;
      e.previous_octave_mode = octave_mode;
      e.previous_repeat = Engine::max_repeat;
      int allocated = 0;
      {
        alloc_counter::scope counter;
        e.update();
        allocated = counter.allocations();
      }
      CHECK(allocated == 0);
      CHECK(e.arpeggio.size() <= Engine::max_steps);
      if(mode == 6)
        CHECK(e.arpeggio.size() == (2 * 128 - 2) * Engine::max_repeat);
      if(mode == 2 && octave_mode == 0)
        CHECK(e.arpeggio.size() == Engine::max_steps);
    }
}

TEST_CASE(
    "Arpeggiator: a chord larger than a step's inline storage plays without allocating",
    "[fx][arpeggiator]")
{
  using Engine = Nodes::Arpeggiator::Engine;
  SECTION("Engine: built again from another pattern, all the octaves")
  {
    Engine e;
    for(int p = 0; p < 128; p++)
      e.notes.insert({(unsigned char)p, 100});
    for(int octave_mode : {0, 1, 2})
      for(int mode : {0, 4, 6, 4})
      {
        INFO("mode " << mode << " octave mode " << octave_mode);
        e.previous_arpeggio = mode;
        e.previous_octave = Engine::max_octave;
        e.previous_octave_mode = octave_mode;
        e.previous_repeat = Engine::max_repeat;
        int allocated = 0;
        {
          alloc_counter::scope counter;
          e.update();
          allocated = counter.allocations();
        }
        CHECK(allocated == 0);
        if(mode == 4)
        {
          CHECK(e.arpeggio.size() == 1);
          CHECK(e.step(0).size() >= 128);
        }
      }
  }

  SECTION("Node: nine keys, each tick")
  {
    Arp a;
    a.prepare({.input_channels = 0, .output_channels = 0, .frames = 100, .rate = 1000.});
    a.inputs.rate.value = 0.125f;
    a.inputs.rate.sync = true;
    a.inputs.arpeggios.value = 4;
    a.inputs.midi.midi_messages.reserve(16);
    a.outputs.midi.midi_messages.reserve(256);
    std::vector<int> per_tick;
    Listener out;
    int64_t pos = 0;
    for(int t = 0; t < 10; t++)
    {
      a.inputs.midi.midi_messages.clear();
      if(t == 0)
        for(int i = 0; i < 9; i++)
          a.inputs.midi.midi_messages.push_back(note_on(60 + i, 100));
      if(t == 6)
        for(int i = 0; i < 9; i++)
          a.inputs.midi.midi_messages.push_back(note_off(60 + i));
      a.outputs.midi.midi_messages.clear();
      halp::tick_musical tk{};
      tk.frames = 100;
      tk.position_in_frames = pos;
      tk.start_position_in_quarters = pos / 500.;
      tk.end_position_in_quarters = (pos + 100) / 500.;
      tk.tempo = 120.;
      tk.signature = {4, 4};
      int allocated = 0;
      {
        alloc_counter::scope counter;
        a(tk);
        allocated = counter.allocations();
      }
      per_tick.push_back(allocated);
      for(auto& m : a.outputs.midi.midi_messages)
        out.feed(m);
      pos += 100;
    }
    CHECK(per_tick == std::vector<int>(10, 0));
    CHECK(out.errors.empty());
    CHECK(out.still_sounding() == 0);
  }
}
