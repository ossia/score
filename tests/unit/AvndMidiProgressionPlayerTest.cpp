// Midi progression player (mtk::MidiProgressionPlayer): chords on the musical
// grid, on trigger and on note; snapping and degree keys; outputs; every note
// it starts, it stops, across ticks of any size, tempo changes, rewinds, all
// notes off and stop.

#include <Advanced/MidiScaler/MidiProgressionPlayer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace
{
using Player = mtk::MidiProgressionPlayer;

libremidi::message note_on(int pitch, int vel = 100, int chan = 1)
{
  return libremidi::channel_events::note_on(chan, pitch, vel);
}
libremidi::message note_off(int pitch, int chan = 1)
{
  return libremidi::channel_events::note_off(chan, pitch, 0);
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

Player::progression_choice choice_of(std::string_view symbols)
{
  for(int i = 0; i < mtk::progression_count; i++)
    if(mtk::progression_table[i].symbols == symbols)
      return Player::progression_choice(i + 1);
  FAIL("no progression " << symbols);
  return {};
}

/**
 * Runs the player at 48 kHz on a timeline whose musical position is the
 * integral of the tempo, the way score reports it: quarters, bars of the
 * signature from 0.
 */
struct Rig
{
  Player node;
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::string> errors;
  std::vector<Event> events;
  std::vector<libremidi::message> others;
  int64_t pos{};
  double quarter{};
  double q_anchor{};
  int64_t pos_anchor{};
  double tempo{120.};
  int sig_num{4};
  int sig_den{4};
  int buffer{};
  //! Note value of a synced step, in whole notes; < 0: free, in seconds.
  double step_whole{1.};

  explicit Rig(int buffer = 512)
      : buffer{buffer}
  {
    node.prepare({.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = 48000.});
    set_step(1.);
  }

  void set_step(double whole_notes)
  {
    step_whole = whole_notes;
    node.inputs.step.sync = true;
    node.inputs.step.value = float(4. * whole_notes * 60. / tempo);
  }
  void set_tempo(double t)
  {
    q_anchor = quarter;
    pos_anchor = pos;
    tempo = t;
    if(node.inputs.step.sync)
      set_step(step_whole);
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

    const double qib = 4. * sig_num / sig_den;
    halp::tick_musical tk{};
    tk.frames = frames;
    tk.position_in_frames = pos;
    tk.tempo = tempo;
    tk.signature = {sig_num, sig_den};
    tk.start_position_in_quarters = quarter;
    // From the last tempo change, not accumulated tick by tick: a grid point on
    // a tick boundary stays on it
    tk.end_position_in_quarters = q_anchor + (pos + frames - pos_anchor) * tempo / 60. / 48000.;
    tk.last_signature_change = 0.;
    tk.bar_at_start = std::floor(tk.start_position_in_quarters / qib + 1e-9) * qib;
    tk.bar_at_end = std::floor(tk.end_position_in_quarters / qib + 1e-9) * qib;
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
      else
        others.push_back(m);
      events.push_back(
          {pos + m.timestamp, status, m.get_channel(), m.size() > 1 ? m.bytes[1] : 0,
           m.size() > 2 ? m.bytes[2] : 0});
    }
    pos += frames;
    quarter = tk.end_position_in_quarters;
  }

  void run(int64_t until, const std::vector<std::pair<int64_t, libremidi::message>>& in = {})
  {
    while(pos < until)
      tick(int(std::min<int64_t>(buffer, until - pos)), in);
  }

  void rewind()
  {
    pos = 0;
    quarter = 0;
    q_anchor = 0;
    pos_anchor = 0;
  }
  void jump(int64_t p, double q)
  {
    pos = pos_anchor = p;
    quarter = q_anchor = q;
  }

  void stop()
  {
    node.stop();
    tick(0);
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

  //! The pitches started at each date, in order.
  std::vector<std::pair<int64_t, std::vector<int>>> chords() const
  {
    std::vector<std::pair<int64_t, std::vector<int>>> r;
    for(auto& e : events)
      if(e.status == 0x90)
      {
        if(r.empty() || r.back().first != e.at)
          r.push_back({e.at, {}});
        r.back().second.push_back(e.pitch);
      }
    for(auto& [at, p] : r)
      std::sort(p.begin(), p.end());
    return r;
  }

  void check_clean()
  {
    INFO((errors.empty() ? std::string{} : errors[0]));
    CHECK(errors.empty());
  }
};

using pitches = std::vector<int>;
}

TEST_CASE("Every bar: I V vi IV with voice leading, on the bar lines", "[progression_player]")
{
  for(int buffer : {64, 100, 512})
  {
    INFO("buffer " << buffer);
    Rig r{buffer};
    r.node.inputs.progression.value = choice_of("I V vi IV");
    r.run(96000 * 5);
    const auto c = r.chords();
    REQUIRE(c.size() == 5);
    for(int i = 0; i < 5; i++)
      CHECK(c[i].first == 96000 * i);
    CHECK(c[0].second == pitches{60, 64, 67});
    CHECK(c[1].second == pitches{59, 62, 67});
    CHECK(c[2].second == pitches{60, 64, 69});
    CHECK(c[3].second == pitches{60, 65, 69});
    CHECK(c[4].second == pitches{60, 64, 67});
    // Each chord sounds until the next: three notes at a time
    CHECK(r.still_sounding() == 3);
    r.stop();
    CHECK(r.still_sounding() == 0);
    r.check_clean();
  }
}

TEST_CASE("A 1/16 step at 120 BPM lands on every 6000th frame", "[progression_player]")
{
  for(int buffer : {64, 100, 512})
  {
    INFO("buffer " << buffer);
    Rig r{buffer};
    r.set_step(1. / 16.);
    r.node.inputs.progression.value = choice_of("ii V I");
    r.run(48000 * 4);
    const auto c = r.chords();
    REQUIRE(c.size() == 32);
    for(std::size_t i = 0; i < c.size(); i++)
      CHECK(c[i].first == int64_t(6000 * i));
    r.stop();
    CHECK(r.still_sounding() == 0);
    r.check_clean();
  }
}

TEST_CASE("The grid stays locked across tempo changes", "[progression_player]")
{
  for(int buffer : {64, 100, 512})
  {
    INFO("buffer " << buffer);
    Rig r{buffer};
    r.set_step(1. / 16.);
    std::vector<double> starts; // musical position of each chord
    double last_q = -1;
    int64_t seen = 0;
    for(int k = 0; k < 40; k++)
    {
      r.set_tempo(k % 3 == 0 ? 120. : (k % 3 == 1 ? 97. : 143.));
      const double q0 = r.quarter;
      const int64_t p0 = r.pos;
      const double tempo = r.tempo;
      r.run(r.pos + 4000);
      const auto c = r.chords();
      for(std::size_t i = seen; i < c.size(); i++)
      {
        const double q = q0 + (c[i].first - p0) * tempo / 60. / 48000.;
        starts.push_back(q);
      }
      seen = c.size();
      (void)last_q;
    }
    REQUIRE(starts.size() > 20);
    for(std::size_t i = 0; i < starts.size(); i++)
    {
      INFO("chord " << i << " at " << starts[i]);
      // On a sixteenth, within a frame
      const double sixteenths = starts[i] * 4.;
      CHECK(std::abs(sixteenths - std::round(sixteenths)) < 4. * 143. / 60. / 48000. + 1e-9);
      CHECK(std::llround(sixteenths) == int64_t(i));
    }
    r.stop();
    r.check_clean();
  }
}

TEST_CASE("Steps shorter than a whole note restart at each bar line", "[progression_player]")
{
  Rig r{256};
  r.sig_num = 3;
  r.set_step(0.5); // half notes in 3/4: beats 1 and 3 of each bar
  r.run(48000 * 4); // 8 quarters
  const auto c = r.chords();
  std::vector<int64_t> at;
  for(auto& [t, p] : c)
    at.push_back(t);
  // Quarters 0, 2, 3, 5, 6 at 24000 frames a quarter
  CHECK(at == std::vector<int64_t>{0, 48000, 72000, 120000, 144000});
  r.stop();
  r.check_clean();
}

TEST_CASE("Free steps in seconds", "[progression_player]")
{
  Rig r{100};
  r.node.inputs.step.sync = false;
  r.node.inputs.step.value = 0.5f;
  r.run(48000 * 2);
  const auto c = r.chords();
  REQUIRE(c.size() == 4);
  for(int i = 0; i < 4; i++)
    CHECK(c[i].first == 24000 * i);
  r.stop();
  r.check_clean();
}

TEST_CASE("Playing from the middle gives the chord of that place", "[progression_player]")
{
  Rig r{512};
  r.node.inputs.progression.value = choice_of("I V vi IV");
  r.node.inputs.inversion.value = mtk::inversion_mode::root;
  // Start two and a half bars in: chord 2 (vi) at once, chord 3 at bar 3
  r.jump(96000 * 2 + 48000, 10.);
  r.run(r.pos + 96000);
  const auto c = r.chords();
  REQUIRE(c.size() == 2);
  CHECK(c[0].first == 96000 * 2 + 48000);
  CHECK(c[0].second == pitches{57, 60, 64});
  CHECK(c[1].first == 96000 * 3);
  CHECK(c[1].second == pitches{53, 57, 60});
  CHECK(r.node.outputs.index.value == 3);
  r.stop();
  r.check_clean();
}

TEST_CASE("A rewind restarts the take, the same way", "[progression_player]")
{
  Rig r{100};
  r.node.inputs.progression.value = choice_of("I iii vi IV");
  r.run(96000 * 3 + 1000);
  const auto first = r.chords();
  r.rewind();
  const auto before = r.events.size();
  r.run(96000 * 3 + 1000);
  // The rewind ended what sounded, before anything else
  CHECK(r.events[before].status == 0x80);
  auto second = r.chords();
  second.erase(second.begin(), second.begin() + first.size());
  REQUIRE(second.size() == first.size());
  for(std::size_t i = 0; i < first.size(); i++)
  {
    CHECK(second[i].second == first[i].second);
    CHECK(second[i].first == first[i].first);
  }
  r.stop();
  r.check_clean();
}

TEST_CASE("Gate shortens the chords", "[progression_player]")
{
  Rig r{128};
  r.node.inputs.gate.value = 50;
  r.run(96000 * 2);
  std::vector<int64_t> offs;
  for(auto& e : r.events)
    if(e.status == 0x80)
      offs.push_back(e.at);
  REQUIRE(offs.size() == 6);
  CHECK(offs[0] == 48000);
  CHECK(offs[3] == 96000 + 48000);
  CHECK(r.still_sounding() == 0);
  r.check_clean();
}

TEST_CASE("Chord, degree, index and root outputs", "[progression_player]")
{
  Rig r{512};
  r.node.inputs.progression.value = Player::progression_choice(0);
  r.node.inputs.custom.value = "vi7 bVIIM I/bVII X V7";
  r.run(1000);
  CHECK(r.node.outputs.chord.value == "Am7");
  CHECK(r.node.outputs.degree.value == "vi7");
  CHECK(r.node.outputs.index.value == 0);
  CHECK(r.node.outputs.root.value == 9);
  CHECK(r.node.outputs.error.value.empty());
  r.run(96000 + 1000);
  CHECK(r.node.outputs.chord.value == "Bb");
  CHECK(r.node.outputs.degree.value == "bVIIM");
  CHECK(r.node.outputs.root.value == 10);
  r.run(2 * 96000 + 1000);
  CHECK(r.node.outputs.chord.value == "C/Bb");
  r.run(3 * 96000 + 1000);
  // A rest: nothing sounds
  CHECK(r.node.outputs.chord.value.empty());
  CHECK(r.node.outputs.degree.value == "X");
  CHECK(r.node.outputs.index.value == 3);
  CHECK(r.node.outputs.root.value == -1);
  CHECK(r.still_sounding() == 0);
  r.run(4 * 96000 + 1000);
  CHECK(r.node.outputs.chord.value == "G7");
  CHECK(r.node.outputs.index.value == 4);
  r.run(5 * 96000 + 1000);
  CHECK(r.node.outputs.index.value == 0);
  r.stop();
  r.check_clean();
}

TEST_CASE("A custom text that does not parse shows why, the last one keeps playing", "[progression_player]")
{
  Rig r{512};
  r.node.inputs.progression.value = Player::progression_choice(0);
  r.node.inputs.custom.value = "ii V I";
  r.run(1000);
  CHECK(r.node.progression().count == 3);
  r.node.inputs.custom.value = "ii V Ixx";
  r.run(2000);
  CHECK(r.node.outputs.error.value == "Unknown chord type: Ixx (at 5)");
  CHECK(r.node.progression().count == 3);
  r.run(96000 + 1000);
  CHECK(r.node.outputs.degree.value == "V");
  r.node.inputs.custom.value = "IV";
  r.run(96000 + 2000);
  CHECK(r.node.outputs.error.value.empty());
  CHECK(r.node.progression().count == 1);
  r.stop();
  r.check_clean();
}

TEST_CASE("Key and mode: Auto plays minor progressions in minor", "[progression_player]")
{
  Rig r{512};
  r.node.inputs.progression.value = choice_of("i VI III VII");
  r.node.inputs.key.value = decltype(r.node.inputs.key)::enum_type::A;
  r.node.inputs.inversion.value = mtk::inversion_mode::root;
  r.run(1000);
  CHECK(r.node.outputs.chord.value == "Am");
  CHECK(r.chords()[0].second == pitches{57, 60, 64});
  // Forced major: i is A major
  r.node.inputs.mode.value = Player::key_mode::major;
  r.run(96000 * 4 + 1000);
  CHECK(r.node.outputs.chord.value == "A");
  r.stop();
  r.check_clean();
}

TEST_CASE("On trigger: Next plays the next chord at once", "[progression_player]")
{
  Rig r{256};
  r.node.inputs.advance.value = Player::advance_mode::on_trigger;
  r.node.inputs.progression.value = choice_of("ii V I");
  r.run(48000);
  CHECK(r.ons().empty());
  for(int k = 0; k < 4; k++)
  {
    r.node.inputs.next.value.emplace();
    const int64_t at = r.pos;
    r.tick(256);
    CHECK(!r.node.inputs.next.value);
    const auto c = r.chords();
    REQUIRE(int(c.size()) == k + 1);
    CHECK(c.back().first == at);
    r.run(r.pos + 10000);
  }
  CHECK(r.node.outputs.degree.value == "ii");
  r.stop();
  CHECK(r.still_sounding() == 0);
  r.check_clean();
}

TEST_CASE("On note: each note-on steps, consumed or passed", "[progression_player]")
{
  for(bool consume : {true, false})
  {
    INFO("consume " << consume);
    Rig r{100};
    r.node.inputs.advance.value = Player::advance_mode::on_note;
    r.node.inputs.consume.value = consume;
    r.node.inputs.channel.value = 2;
    r.node.inputs.progression.value = choice_of("ii V I");
    r.run(
        20000, {{1000, note_on(40)},
                {3000, note_off(40)},
                {5000, note_on(41)},
                {5050, note_on(42)},
                {7000, note_off(41)},
                {7000, note_off(42)}});
    std::vector<Event> chord_ons, input_ons;
    for(auto& e : r.ons())
      (e.chan == 2 ? chord_ons : input_ons).push_back(e);
    CHECK(chord_ons.size() == 9);
    CHECK(chord_ons[0].at == 1000);
    CHECK(chord_ons[3].at == 5000);
    CHECK(chord_ons[6].at == 5050);
    CHECK(r.node.outputs.degree.value == "I");
    if(consume)
      CHECK(input_ons.empty());
    else
      CHECK(input_ons.size() == 3);
    // Only the last chord still sounds
    CHECK(r.still_sounding() == 3);
    r.stop();
    CHECK(r.still_sounding() == 0);
    r.check_clean();
  }
}

TEST_CASE("Snap to chord keeps each note with its own note-off", "[progression_player]")
{
  Rig r{100};
  r.node.inputs.chords.value = false;
  r.node.inputs.input.value = Player::input_mode::snap_chord;
  r.node.inputs.progression.value = choice_of("I V vi IV");
  // 62 (D) over C major goes down to C; held across the change to G major,
  // its note-off still ends 60
  r.run(
      96000 * 2, {{1000, note_on(62)},
                  {2000, note_on(66)},
                  {3000, note_off(66)},
                  {97000, note_on(64)},
                  {98000, note_off(62)},
                  {99000, note_off(64)}});
  std::vector<std::pair<int, int>> got;
  for(auto& e : r.events)
    got.push_back({e.status, e.pitch});
  CHECK(
      got
      == std::vector<std::pair<int, int>>{
          {0x90, 60}, {0x90, 67}, {0x80, 67}, {0x90, 62}, {0x80, 60}, {0x80, 62}});
  CHECK(r.still_sounding() == 0);
  r.check_clean();
}

TEST_CASE("Snap to scale", "[progression_player]")
{
  Rig r{100};
  r.node.inputs.chords.value = false;
  r.node.inputs.input.value = Player::input_mode::snap_scale;
  r.node.inputs.key.value = decltype(r.node.inputs.key)::enum_type::D;
  r.run(10000, {{100, note_on(60)}, {200, note_on(61)}, {300, note_off(60)}, {400, note_off(61)}});
  std::vector<int> ons;
  for(auto& e : r.ons())
    ons.push_back(e.pitch);
  CHECK(ons == std::vector<int>{59, 61}); // C goes down to B, C# is in D major
  CHECK(r.still_sounding() == 0);
  r.check_clean();
}

TEST_CASE("Keys play degrees: white keys from C4 climb the chord", "[progression_player]")
{
  Rig r{100};
  r.node.inputs.chords.value = false;
  r.node.inputs.input.value = Player::input_mode::degrees;
  r.node.inputs.progression.value = Player::progression_choice(0);
  r.node.inputs.custom.value = "vi7";
  std::vector<std::pair<int64_t, libremidi::message>> in;
  int64_t t = 100;
  for(int p : {60, 62, 64, 65, 67, 59, 48, 61})
  {
    in.push_back({t, note_on(p)});
    in.push_back({t + 10, note_off(p)});
    t += 100;
  }
  r.run(10000, in);
  std::vector<int> ons;
  for(auto& e : r.ons())
    ons.push_back(e.pitch);
  // Am7: A C E G from A3 (57, the A nearest C4); B3 is one tone below; C3,
  // seven white keys down, is seven tones down (two chord octaves less one
  // tone); C#4 is black: silent
  CHECK(ons == std::vector<int>{57, 60, 64, 67, 69, 55, 36});
  CHECK(r.still_sounding() == 0);
  r.check_clean();
}

TEST_CASE("Other messages pass unchanged and in order", "[progression_player]")
{
  Rig r{100};
  r.node.inputs.chords.value = false;
  std::vector<std::pair<int64_t, libremidi::message>> in{
      {10, libremidi::channel_events::control_change(1, 7, 100)},
      {20, libremidi::channel_events::pitch_bend(3, 9000)},
      {20, libremidi::channel_events::program_change(2, 5)},
      {150, libremidi::channel_events::aftertouch(1, 33)},
      {160, note_off(77)},
  };
  r.run(1000, in);
  REQUIRE(r.others.size() == 4);
  for(int i = 0; i < 4; i++)
  {
    CHECK(r.others[i].bytes == in[i].second.bytes);
  }
  // A note-off of no note received is dropped
  CHECK(r.events.size() == 4);
}

TEST_CASE("All notes off ends the chord and the held notes of its channel", "[progression_player]")
{
  Rig r{100};
  r.run(
      2000, {{100, note_on(70)},
             {200, note_on(71, 100, 2)},
             {500, libremidi::channel_events::control_change(1, 123, 0)}});
  CHECK(r.still_sounding() == 1); // the one on channel 2
  // Their note-offs, later, do not go out again
  r.run(4000, {{3000, note_off(70)}, {3100, note_off(71, 2)}});
  CHECK(r.still_sounding() == 0);
  r.check_clean();
}

TEST_CASE("Every note-on gets one note-off, whatever happens", "[progression_player]")
{
  std::mt19937 rng{1234};
  for(int round = 0; round < 30; round++)
  {
    const int buffer = std::vector<int>{1, 7, 64, 100, 512}[round % 5];
    Rig r{buffer};
    std::uniform_int_distribution<int> pitch{40, 80}, chan{1, 3}, mode{0, 3}, adv{0, 2},
        prog{0, mtk::progression_count};
    std::vector<std::pair<int64_t, libremidi::message>> in;
    int64_t t = 0;
    std::vector<std::pair<int, int>> held;
    for(int k = 0; k < 400; k++)
    {
      t += rng() % 700;
      const int what = rng() % 10;
      if(what < 5 || held.empty())
      {
        const int p = pitch(rng), c = chan(rng);
        in.push_back({t, note_on(p, 90, c)});
        held.push_back({p, c});
      }
      else if(what < 9)
      {
        const auto i = rng() % held.size();
        in.push_back({t, note_off(held[i].first, held[i].second)});
        held.erase(held.begin() + i);
      }
      else
        in.push_back({t, libremidi::channel_events::control_change(chan(rng), 123, 0)});
    }
    r.set_step(1. / 8.);
    int64_t until = 0;
    while(until < t + 1000)
    {
      until += 5000;
      r.node.inputs.input.value = Player::input_mode(mode(rng));
      r.node.inputs.advance.value = Player::advance_mode(adv(rng));
      r.node.inputs.progression.value = Player::progression_choice(prog(rng));
      r.node.inputs.custom.value = (rng() % 2) ? "I  bVIIM/V X ii7 V7" : "IVxx";
      r.node.inputs.chords.value = rng() % 4 != 0;
      r.node.inputs.bass.value = rng() % 2;
      r.node.inputs.channel.value = chan(rng);
      r.node.inputs.gate.value = 1 + rng() % 100;
      r.node.inputs.notes.value = 1 + rng() % 6;
      r.node.inputs.center.value = 24 + rng() % 84;
      if(rng() % 3 == 0)
        r.node.inputs.next.value.emplace();
      if(rng() % 7 == 0)
        r.set_tempo(60 + rng() % 120);
      r.run(until, in);
      if(rng() % 13 == 0)
        r.rewind();
    }
    // The upstream releases what it still holds, then the transport stops
    for(auto& [p, c] : held)
      in.push_back({r.pos, note_off(p, c)});
    r.tick(buffer, in);
    r.stop();
    INFO("round " << round);
    CHECK(r.still_sounding() == 0);
    CHECK(r.node.sounding_notes() == 0);
    r.check_clean();
  }
}

TEST_CASE("Stop in a 0-frame tick ends everything at 0", "[progression_player]")
{
  Rig r{512};
  r.node.inputs.bass.value = true;
  r.run(5000, {{100, note_on(70)}});
  CHECK(r.still_sounding() == 5);
  const auto before = r.events.size();
  r.stop();
  CHECK(r.events.size() == before + 5);
  CHECK(r.still_sounding() == 0);
  // After a stop, the progression starts over
  r.rewind();
  r.run(1000);
  CHECK(r.node.outputs.index.value == 0);
  r.stop();
  r.check_clean();
}
