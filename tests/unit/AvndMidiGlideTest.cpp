// Midi glide (mo::MidiGlide): every note-off goes on the channel of its
// note-on, through retriggers, legato across channels, a stop and the
// receiver's own all-notes-off, without allocating.

#include "AllocCounter.hpp"

#include <Midi/MidiGlide.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Glide = mo::MidiGlide;

libremidi::message on(int ch, int p, int v = 100, int64_t ts = 0)
{
  auto m = libremidi::channel_events::note_on(ch, p, v);
  m.timestamp = ts;
  return m;
}
libremidi::message off(int ch, int p, int v = 0, int64_t ts = 0)
{
  auto m = libremidi::channel_events::note_off(ch, p, v);
  m.timestamp = ts;
  return m;
}

bool is_on(const libremidi::message& m)
{
  return m.get_message_type() == libremidi::message_type::NOTE_ON && m.bytes[2] != 0;
}
bool is_off(const libremidi::message& m)
{
  return m.get_message_type() == libremidi::message_type::NOTE_OFF
         || (m.get_message_type() == libremidi::message_type::NOTE_ON && m.bytes[2] == 0);
}

//! What a synth on the output would sound: a note per (channel, pitch).
struct Listener
{
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;

  void feed(const libremidi::message& m)
  {
    if(m.get_message_type() == libremidi::message_type::CONTROL_CHANGE
       && (m.bytes[1] == 120 || m.bytes[1] == 123))
    {
      for(auto& [k, n] : sounding)
        if(k.first == m.get_channel())
          n = 0;
      return;
    }
    if(!is_on(m) && !is_off(m))
      return;
    const auto key = std::make_pair(m.get_channel(), int(m.bytes[1]));
    auto& n = sounding[key];
    const auto name = "ch " + std::to_string(key.first) + " pitch " + std::to_string(key.second);
    if(is_on(m))
    {
      if(n > 0)
        errors.push_back("started twice: " + name);
      n++;
    }
    else
    {
      if(n == 0)
        errors.push_back("stopped while silent: " + name);
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
  Glide node;
  Listener out;
  std::vector<libremidi::message> last;
  int allocations = 0;

  Rig()
  {
    node.inputs.in.midi_messages.reserve(256);
    node.outputs.out.midi_messages.reserve(512);
    last.reserve(512);
  }

  const std::vector<libremidi::message>& tick(const std::vector<libremidi::message>& in = {})
  {
    node.inputs.in.midi_messages.assign(in.begin(), in.end());
    node.outputs.out.midi_messages.clear();
    {
      alloc_counter::scope counter;
      node();
      allocations += counter.allocations();
    }
    last.assign(node.outputs.out.midi_messages.begin(), node.outputs.out.midi_messages.end());
    for(auto& m : last)
      out.feed(m);
    return last;
  }

  //! The (channel, pitch) of each note-off of the last tick.
  std::vector<std::pair<int, int>> offs() const
  {
    std::vector<std::pair<int, int>> r;
    for(auto& m : last)
      if(is_off(m))
        r.emplace_back(m.get_channel(), m.bytes[1]);
    return r;
  }
};
using offs_t = std::vector<std::pair<int, int>>;
}

TEST_CASE("Midi glide: a new note ends the previous on its own channel", "[avnd][midi][glide]")
{
  Rig r;
  r.tick({on(1, 60)});
  auto& t = r.tick({on(2, 64, 90, 7)});
  REQUIRE(t.size() == 2);
  CHECK(is_off(t[0]));
  CHECK(t[0].get_channel() == 1);
  CHECK(t[0].bytes[1] == 60);
  CHECK(t[0].timestamp == 7);
  CHECK(is_on(t[1]));
  CHECK(t[1].get_channel() == 2);

  // Its own release ends the new note on its channel.
  r.tick({off(2, 64)});
  CHECK(r.offs() == offs_t{{2, 64}});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi glide: a release on another channel leaves the note sounding", "[avnd][midi][glide]")
{
  Rig r;
  r.tick({on(1, 60)});
  CHECK(r.tick({off(2, 60)}).empty());
  CHECK(r.node.sounding());
  CHECK(r.out.still_sounding() == 1);
  r.tick({off(1, 60, 40, 3)});
  REQUIRE(r.offs() == offs_t{{1, 60}});
  CHECK(r.last[0].bytes[2] == 40); // the release velocity is kept
  CHECK(r.last[0].timestamp == 3);
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi glide: legato across channels in one tick", "[avnd][midi][glide]")
{
  Rig r;
  r.tick({on(1, 60, 100, 0), on(3, 62, 100, 2), off(1, 60, 0, 3), on(5, 60, 100, 4),
          off(3, 62, 0, 5)});
  // 60 on 1 ends when 62 starts, 62 on 3 when 60 on 5 starts; the late
  // releases of the ended keys do nothing.
  CHECK(r.offs() == offs_t{{1, 60}, {3, 62}});
  CHECK(r.out.still_sounding() == 1);
  CHECK(r.out.sounding[{5, 60}] == 1);
  // A note-on with velocity 0 releases it.
  r.tick({on(5, 60, 0)});
  CHECK(r.offs() == offs_t{{5, 60}});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi glide: the same key struck again restarts on its channel", "[avnd][midi][glide]")
{
  Rig r;
  r.tick({on(4, 60)});
  r.tick({on(4, 60, 70)});
  CHECK(r.offs() == offs_t{{4, 60}});
  CHECK(r.out.still_sounding() == 1);
  r.tick({off(4, 60)});
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi glide: stop ends the sounding note", "[avnd][midi][glide]")
{
  Rig r;
  r.tick({on(3, 60)});
  r.node.stop();
  r.tick();
  CHECK(r.offs() == offs_t{{3, 60}});
  CHECK(r.out.still_sounding() == 0);
  CHECK(!r.node.sounding());

  // The key's release after the stop has nothing to end, and the next note
  // starts without ending anything.
  CHECK(r.tick({off(3, 60)}).empty());
  CHECK(r.tick({on(1, 50)}).size() == 1);
  // A stop with nothing sounding sends nothing.
  r.tick({off(1, 50)});
  r.node.stop();
  CHECK(r.tick().empty());
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi glide: all notes off on the note's channel ends it", "[avnd][midi][glide]")
{
  Rig r;
  r.tick({on(1, 60)});
  // On another channel it does not.
  r.tick({libremidi::channel_events::control_change(2, 123, 0)});
  CHECK(r.node.sounding());
  auto& t = r.tick({libremidi::channel_events::control_change(1, 123, 0)});
  REQUIRE(t.size() == 1); // passed on
  CHECK(!r.node.sounding());
  CHECK(r.tick({on(1, 62)}).size() == 1);
  r.tick({libremidi::channel_events::control_change(1, 120, 0)});
  CHECK(!r.node.sounding());
  CHECK(r.tick({off(1, 62)}).empty());
  CHECK(r.out.errors.empty());
}

TEST_CASE("Midi glide: random play pairs every note and does not allocate", "[avnd][midi][glide]")
{
  Rig r;
  std::mt19937 rng{1234};
  std::uniform_int_distribution<int> ch{1, 4}, pitch{58, 63}, kind{0, 9}, count{0, 6};
  std::vector<libremidi::message> in;
  in.reserve(16);
  for(int t = 0; t < 2000; t++)
  {
    in.clear();
    for(int i = 0, n = count(rng); i < n; i++)
    {
      const int k = kind(rng);
      if(k < 5)
        in.push_back(on(ch(rng), pitch(rng), 100, i));
      else if(k < 9)
        in.push_back(off(ch(rng), pitch(rng), 0, i));
      else
        in.push_back(on(ch(rng), pitch(rng), 0, i));
    }
    r.tick(in);
    CHECK(r.out.still_sounding() <= 1);
    if(t % 97 == 0)
    {
      r.node.stop();
      r.tick();
      CHECK(r.out.still_sounding() == 0);
    }
  }
  r.node.stop();
  r.tick();
  CHECK(r.out.still_sounding() == 0);
  CHECK(r.out.errors.empty());
  CHECK(r.allocations == 0);
}
