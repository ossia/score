// Midi random sequencer (mtk::MidiRandomSequencer): the steps stay on the
// musical grid across buffer sizes and tempo changes, the Turing and Marbles
// engines loop, mutate and shape as documented, every note it starts it
// stops, seeded takes are reproducible, and operator() never allocates.

#include <Advanced/MidiScaler/MidiRandomSequencer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <map>
#include <new>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
std::atomic<bool> g_count_allocations{false};
std::atomic<int> g_allocations{0};
}

void* operator new(std::size_t n)
{
  if(g_count_allocations.load(std::memory_order_relaxed))
    g_allocations++;
  if(void* p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc{};
}
void operator delete(void* p) noexcept
{
  std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
  std::free(p);
}

namespace
{
using Seq = mtk::MidiRandomSequencer;
constexpr double rate = 48000.;

struct Event
{
  int64_t at;
  int status;
  int chan;
  int d1;
  int d2;
  bool operator==(const Event&) const = default;
};

std::ostream& operator<<(std::ostream& s, const Event& e)
{
  return s << "{" << e.at << ", " << std::hex << e.status << std::dec << ", " << e.chan
           << ", " << e.d1 << ", " << e.d2 << "}";
}

using Input = std::vector<std::pair<int64_t, libremidi::message>>;

//! Runs the node with a musical clock that follows `tempo`, which may change
//! between ticks.
struct Rig
{
  Seq node;
  int buffer{512};
  double tempo{120.};
  int64_t pos{};
  double quarters{};
  std::vector<Event> events;
  std::vector<std::string> errors;
  std::map<std::pair<int, int>, int> sounding;
  std::vector<std::pair<int64_t, float>> values;
  std::vector<std::pair<int64_t, bool>> gates;
  int allocations{};

  explicit Rig(int buffer = 512)
      : buffer{buffer}
  {
    node.prepare({.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = rate});
    node.inputs.midi.midi_messages.reserve(256);
    node.outputs.midi.midi_messages.reserve(4096);
    set_rate_note(1. / 16.);
  }

  //! A synced Rate of `whole` notes, in seconds at the current tempo, as the
  //! host hands it over.
  void set_rate_note(double whole)
  {
    node.inputs.rate.sync = true;
    node.inputs.rate.value = float(4. * whole * 60. / tempo);
    m_rate_note = whole;
  }

  void set_tempo(double t)
  {
    m_q0 = quarters;
    m_pos0 = pos;
    tempo = t;
    if(node.inputs.rate.sync)
      set_rate_note(m_rate_note);
  }

  void tick(int frames, const Input& in = {})
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
    tk.tempo = tempo;
    tk.signature = {4, 4};
    tk.start_position_in_quarters = quarters;
    const double end_q = m_q0 + double(pos + frames - m_pos0) / rate * tempo / 60.;
    tk.end_position_in_quarters = end_q;

    g_allocations = 0;
    g_count_allocations = true;
    node(tk);
    g_count_allocations = false;
    allocations += g_allocations;

    node.inputs.step.value.reset();
    node.inputs.reset.value.reset();

    int64_t last = 0;
    for(auto& m : node.outputs.midi.midi_messages)
    {
      if(frames > 0 && (m.timestamp < 0 || m.timestamp >= frames))
        errors.push_back("timestamp " + std::to_string(m.timestamp) + " outside the tick");
      if(frames == 0 && m.timestamp != 0)
        errors.push_back("timestamp in a 0-frame tick");
      if(m.timestamp < last)
        errors.push_back("out of order");
      last = m.timestamp;
      record(m, pos + m.timestamp);
    }
    for(auto& [ts, v] : node.outputs.value.values)
      values.push_back({pos + ts, v});
    node.outputs.value.values.clear();
    for(auto& [ts, v] : node.outputs.gate.values)
      gates.push_back({pos + ts, v});
    node.outputs.gate.values.clear();
    pos += frames;
    quarters = end_q;
  }

  void record(const libremidi::message& m, int64_t at)
  {
    Event e{at, m.bytes[0] & 0xF0, (m.bytes[0] & 0x0F) + 1, m.size() > 1 ? m.bytes[1] : 0,
            m.size() > 2 ? m.bytes[2] : 0};
    if(m.bytes[0] >= 0xF0)
      e.status = m.bytes[0];
    if(e.status == 0x90 && e.d2 == 0)
      e.status = 0x80;
    events.push_back(e);
    if(e.status != 0x90 && e.status != 0x80)
      return;
    auto& n = sounding[{e.chan, e.d1}];
    if(e.status == 0x90)
    {
      if(n > 0)
        errors.push_back("started twice: " + std::to_string(e.d1));
      n++;
    }
    else if(n > 0)
      n--;
    else
      errors.push_back("stopped while silent: " + std::to_string(e.d1));
  }

  void run(int64_t until, const Input& in = {})
  {
    while(pos < until)
      tick(int(std::min<int64_t>(buffer, until - pos)), in);
  }

  void stop()
  {
    node.stop();
    tick(0);
  }

  //! Back to the start of the timeline, as a transport rewind does.
  void rewind()
  {
    pos = 0;
    quarters = 0.;
    m_pos0 = 0;
    m_q0 = 0.;
  }

  std::vector<Event> ons() const
  {
    std::vector<Event> r;
    for(auto& e : events)
      if(e.status == 0x90)
        r.push_back(e);
    return r;
  }
  std::vector<int> pitches() const
  {
    std::vector<int> r;
    for(auto& e : ons())
      r.push_back(e.d1);
    return r;
  }

  void check_clean() const
  {
    INFO((errors.empty() ? std::string{} : errors.front()));
    CHECK(errors.empty());
    int left = 0;
    for(auto& [k, n] : sounding)
      left += n;
    CHECK(left == 0);
    CHECK(allocations == 0);
  }

private:
  double m_rate_note{1. / 16.};
  //! Where the tempo last changed: the musical position is exact from there
  double m_q0{};
  int64_t m_pos0{};
};

//! The smallest period of `v` over its whole length, or 0.
int period_of(const std::vector<int>& v, int max_period)
{
  for(int p = 1; p <= max_period; p++)
  {
    bool ok = true;
    for(std::size_t i = p; i < v.size() && ok; i++)
      ok = v[i] == v[i - p];
    if(ok)
      return p;
  }
  return 0;
}

constexpr int64_t sixteenth = 6000; // at 120 BPM, 48 kHz
}

TEST_CASE("Random sequencer: synced steps land on the grid", "[midi_random_sequencer]")
{
  for(int buffer : {64, 100, 512, 1})
  {
    INFO("buffer " << buffer);
    Rig r{buffer};
    r.node.inputs.seed.value = 7;
    r.run(buffer == 1 ? 4 * sixteenth : 64 * sixteenth);
    r.stop();
    const auto ons = r.ons();
    REQUIRE(ons.size() == (buffer == 1 ? 4u : 64u));
    for(std::size_t i = 0; i < ons.size(); i++)
    {
      INFO("step " << i);
      CHECK(ons[i].at == int64_t(i) * sixteenth);
    }
    r.check_clean();
  }
}

TEST_CASE("Random sequencer: steps stay on the grid across tempo changes", "[midi_random_sequencer]")
{
  Rig r{100};
  r.node.inputs.seed.value = 3;
  // One bar at 120, then 90, then 150
  r.run(4 * 24000);
  r.set_tempo(90.);
  const int64_t q90 = 32000;
  r.run(4 * 24000 + 4 * q90);
  r.set_tempo(150.);
  const int64_t q150 = 19200;
  r.run(4 * 24000 + 4 * q90 + 4 * q150);
  r.stop();

  std::vector<int64_t> want;
  for(int k = 0; k < 16; k++)
    want.push_back(k * 6000);
  for(int k = 0; k < 16; k++)
    want.push_back(96000 + k * q90 / 4);
  for(int k = 0; k < 16; k++)
    want.push_back(96000 + 4 * q90 + k * q150 / 4);
  std::vector<int64_t> got;
  for(auto& e : r.ons())
    got.push_back(e.at);
  CHECK(got == want);
  r.check_clean();
}

TEST_CASE("Random sequencer: free rate does not drift", "[midi_random_sequencer]")
{
  Rig r{100};
  r.node.inputs.rate.sync = false;
  r.node.inputs.rate.value = 0.0101f; // 484.8 frames
  r.run(int64_t(2000 * 484.8) + 10);
  r.stop();
  const auto ons = r.ons();
  REQUIRE(ons.size() == 2001);
  const double step = double(0.0101f) * rate;
  for(std::size_t i = 0; i < ons.size(); i += 97)
  {
    INFO("step " << i);
    CHECK(std::abs(double(ons[i].at) - double(i) * step) < 1.);
  }
  r.check_clean();
}

TEST_CASE("Random sequencer: same seed, same take; rewind and Reset restart it", "[midi_random_sequencer]")
{
  for(auto engine : {Seq::Turing, Seq::Marbles})
  {
    INFO("engine " << int(engine));
    auto setup = [engine](Rig& r) {
      r.node.inputs.engine.value = engine;
      r.node.inputs.seed.value = 1234;
      r.node.inputs.lock.value = 0.5f;
      r.node.inputs.deja_vu.value = 0.5f;
      r.node.inputs.density.value = 0.7f;
      r.node.inputs.jitter.value = 0.6f;
      r.node.inputs.velocity.value = {20.f, 120.f};
      r.node.inputs.octaves.value = 2;
    };
    Rig a{64}, b{512};
    setup(a);
    setup(b);
    a.run(8 * 24000);
    b.run(8 * 24000);
    a.stop();
    b.stop();
    REQUIRE(a.ons().size() > 10);
    CHECK(a.events == b.events);
    a.check_clean();
    b.check_clean();

    // A rewind without a stop restarts the take
    Rig c{100};
    setup(c);
    c.run(8 * 24000);
    auto first = c.ons();
    c.rewind();
    c.events.clear();
    c.run(8 * 24000);
    c.stop();
    auto second = c.ons();
    REQUIRE(second.size() >= first.size() - 1);
    // The jitter may carry the last note of the first pass over the rewind
    for(std::size_t i = 0; i + 1 < first.size(); i++)
    {
      INFO("note " << i);
      CHECK(first[i].d1 == second[i].d1);
      CHECK(first[i].d2 == second[i].d2);
      CHECK(first[i].at == second[i].at);
    }
    c.check_clean();

    // Reset at a bar line restarts it too, from that bar
    Rig d{100};
    setup(d);
    d.run(4 * 24000);
    d.node.inputs.reset.value.emplace();
    d.run(8 * 24000);
    d.stop();
    std::vector<int> p1, p2;
    for(auto& e : d.ons())
      (e.at < 4 * 24000 ? p1 : p2).push_back(e.d1);
    REQUIRE(p1.size() > 4);
    REQUIRE(p2.size() >= p1.size() - 1);
    for(std::size_t i = 0; i + 1 < p1.size(); i++)
      CHECK(p1[i] == p2[i]);
    d.check_clean();
  }

  // Seed 0 is free: two instances differ
  Rig e, f;
  e.node.inputs.lock.value = 0.f;
  f.node.inputs.lock.value = 0.f;
  e.run(4 * 24000);
  f.run(4 * 24000);
  CHECK(e.pitches() != f.pitches());
}

TEST_CASE("Random sequencer: Turing lock", "[midi_random_sequencer]")
{
  SECTION("Lock 1 is a loop of Length steps")
  {
    for(int len : {2, 5, 8, 16})
    {
      INFO("length " << len);
      Rig r;
      r.node.inputs.seed.value = 11 + len;
      r.node.inputs.lock.value = 1.f;
      r.node.inputs.length.value = len;
      r.node.inputs.note_range.value = {36.f, 96.f};
      r.node.inputs.key_mode.value = Seq::scale_control::chromatic;
      r.run(64 * sixteenth);
      r.stop();
      const auto p = r.pitches();
      REQUIRE(p.size() == 64);
      const int per = period_of(p, 32);
      CHECK(per > 0);
      CHECK(len % per == 0);
      r.check_clean();
    }
  }
  SECTION("Lock -1 is the loop and its inverse")
  {
    Rig r;
    r.node.inputs.seed.value = 5;
    r.node.inputs.lock.value = -1.f;
    r.node.inputs.length.value = 8;
    r.node.inputs.note_range.value = {0.f, 127.f};
    r.node.inputs.key_mode.value = Seq::scale_control::chromatic;
    r.run(64 * sixteenth);
    r.stop();
    // The Value output: each step's byte, inverted after Length steps
    REQUIRE(r.values.size() == 64);
    for(std::size_t i = 16; i + 8 < r.values.size(); i++)
    {
      const int a = int(std::lround(r.values[i].second * 255.f));
      const int b = int(std::lround(r.values[i + 8].second * 255.f));
      CHECK(a + b == 255);
    }
    const auto p = r.pitches();
    CHECK(period_of(p, 32) == 16);
    r.check_clean();
  }
  SECTION("Lock 0 does not repeat; Lock 0.9 mutates slowly")
  {
    Rig r;
    r.node.inputs.seed.value = 9;
    r.node.inputs.lock.value = 0.f;
    r.node.inputs.note_range.value = {0.f, 127.f};
    r.node.inputs.key_mode.value = Seq::scale_control::chromatic;
    r.run(64 * sixteenth);
    CHECK(period_of(r.pitches(), 32) == 0);
    const auto all = r.pitches();
    std::set<int> distinct(all.begin(), all.end());
    CHECK(distinct.size() > 20);

    Rig m;
    m.node.inputs.seed.value = 9;
    m.node.inputs.lock.value = 0.9f;
    m.node.inputs.length.value = 8;
    m.node.inputs.note_range.value = {0.f, 127.f};
    m.node.inputs.key_mode.value = Seq::scale_control::chromatic;
    m.run(256 * sixteenth);
    const auto p = m.pitches();
    int same = 0;
    for(std::size_t i = 8; i < p.size(); i++)
      same += p[i] == p[i - 8];
    // About one bit in ten changes; a changed bit touches 8 values
    CHECK(same > int(p.size()) / 4);
    CHECK(same < int(p.size()) - 8);
  }
  SECTION("Write forces ones or zeros")
  {
    Rig r;
    r.node.inputs.write.value = Seq::Ones;
    r.node.inputs.note_range.value = {40.f, 80.f};
    r.run(32 * sixteenth);
    REQUIRE(r.values.size() == 32);
    CHECK(r.values.back().second == 1.f);
    CHECK(r.pitches().back() == 79); // the highest C major note in range
    r.node.inputs.write.value = Seq::Zeros;
    r.run(64 * sixteenth);
    CHECK(r.values.back().second == 0.f);
    CHECK(r.pitches().back() == 40);
    r.stop();
    r.check_clean();
  }
}

TEST_CASE("Random sequencer: Turing gates follow the register bits", "[midi_random_sequencer]")
{
  Rig r;
  r.node.inputs.seed.value = 21;
  r.node.inputs.lock.value = 0.f;
  r.node.inputs.gate.value = Seq::gate_control::bit1;
  r.run(128 * sixteenth);
  r.stop();
  // A step plays exactly when its value's lowest bit is set
  REQUIRE(r.values.size() == 128);
  std::set<int64_t> on_dates;
  for(auto& e : r.ons())
    on_dates.insert(e.at);
  int played = 0;
  for(auto& [at, v] : r.values)
  {
    const bool bit = (int(std::lround(v * 255.f)) & 1) != 0;
    CHECK(on_dates.contains(at) == bit);
    played += bit;
  }
  CHECK(played > 30);
  CHECK(played < 98);
  r.check_clean();
}

TEST_CASE("Random sequencer: Marbles", "[midi_random_sequencer]")
{
  auto marbles = [](Rig& r) {
    r.node.inputs.engine.value = Seq::Marbles;
    r.node.inputs.seed.value = 77;
  };
  SECTION("Deja vu 1 is a loop of Length steps, still shaped by Spread")
  {
    Rig r;
    marbles(r);
    r.node.inputs.deja_vu.value = 1.f;
    r.node.inputs.length.value = 5;
    r.node.inputs.spread.value = 1.f;
    r.node.inputs.note_range.value = {36.f, 96.f};
    r.run(40 * sixteenth);
    auto p = r.pitches();
    REQUIRE(p.size() == 40);
    CHECK(period_of(p, 20) == 5);
    // Spread 0: everything on the Bias note
    r.node.inputs.spread.value = 0.f;
    r.node.inputs.bias.value = 0.f;
    r.events.clear();
    r.run(80 * sixteenth);
    r.stop();
    const auto all = r.pitches();
    std::set<int> distinct(all.begin(), all.end());
    CHECK(distinct == std::set<int>{36});
    r.check_clean();
  }
  SECTION("Deja vu 0 draws new values")
  {
    Rig r;
    marbles(r);
    r.node.inputs.spread.value = 1.f;
    r.node.inputs.note_range.value = {36.f, 96.f};
    r.run(64 * sixteenth);
    CHECK(period_of(r.pitches(), 32) == 0);
  }
  SECTION("Bias moves the notes up")
  {
    double means[2]{};
    for(int i = 0; i < 2; i++)
    {
      Rig r;
      marbles(r);
      r.node.inputs.bias.value = i == 0 ? 0.2f : 0.8f;
      r.node.inputs.spread.value = 0.6f;
      r.node.inputs.note_range.value = {36.f, 96.f};
      r.run(128 * sixteenth);
      for(int p : r.pitches())
        means[i] += p;
      means[i] /= r.pitches().size();
    }
    CHECK(means[1] > means[0] + 15.);
  }
  SECTION("Steps 1 keeps the root and the fifth")
  {
    Rig r;
    marbles(r);
    r.node.inputs.spread.value = 1.f;
    r.node.inputs.steps.value = 1.f;
    r.node.inputs.root.value = Seq::root_control::d;
    r.node.inputs.key_mode.value = Seq::scale_control::dorian;
    r.node.inputs.note_range.value = {36.f, 84.f};
    r.run(128 * sixteenth);
    std::set<int> pcs;
    for(int p : r.pitches())
      pcs.insert(p % 12);
    CHECK(pcs == std::set<int>{2, 9});
  }
  SECTION("Density and Regularity")
  {
    Rig r;
    marbles(r);
    r.node.inputs.density.value = 0.f;
    r.run(32 * sixteenth);
    CHECK(r.ons().empty());

    Rig e;
    marbles(e);
    e.node.inputs.density.value = 0.5f;
    e.node.inputs.regularity.value = 1.f;
    e.node.inputs.length.value = 8;
    e.run(64 * sixteenth);
    std::vector<int64_t> steps;
    for(auto& ev : e.ons())
      steps.push_back(ev.at / sixteenth);
    // Four of eight, evenly: steps 0, 2, 4, 6 of each loop
    REQUIRE(steps.size() == 32);
    for(auto s : steps)
      CHECK(s % 2 == 0);

    Rig c;
    marbles(c);
    c.node.inputs.density.value = 0.5f;
    c.run(256 * sixteenth);
    CHECK(c.ons().size() > 90);
    CHECK(c.ons().size() < 166);
  }
}

TEST_CASE("Random sequencer: jitter only delays, swing delays the off-beats", "[midi_random_sequencer]")
{
  Rig r{100};
  r.node.inputs.engine.value = Seq::Marbles;
  r.node.inputs.seed.value = 4;
  r.node.inputs.jitter.value = 1.f;
  r.run(128 * sixteenth);
  r.stop();
  REQUIRE(r.ons().size() == 128);
  int late = 0;
  for(std::size_t i = 0; i < r.ons().size(); i++)
  {
    const int64_t d = r.ons()[i].at - int64_t(i) * sixteenth;
    CHECK(d >= 0);
    CHECK(d <= sixteenth / 2);
    late += d > 100;
  }
  CHECK(late > 64);
  r.check_clean();

  Rig s{100};
  s.node.inputs.seed.value = 4;
  s.node.inputs.swing.value = 0.5f;
  s.run(16 * sixteenth);
  s.stop();
  REQUIRE(s.ons().size() == 16);
  for(int i = 0; i < 16; i++)
    CHECK(s.ons()[i].at == i * sixteenth + (i % 2 ? sixteenth / 3 : 0));
  s.check_clean();
}

TEST_CASE("Random sequencer: notes, gate length, velocity, accent, channel", "[midi_random_sequencer]")
{
  Rig r;
  r.node.inputs.seed.value = 8;
  r.node.inputs.lock.value = 0.f;
  r.node.inputs.gate_length.value = 0.25f;
  r.node.inputs.velocity.value = {40.f, 100.f};
  r.node.inputs.channel.value = 10;
  r.run(64 * sixteenth);
  r.stop();
  for(auto& e : r.events)
  {
    CHECK(e.chan == 10);
    if(e.status == 0x90)
    {
      CHECK(e.d2 >= 40);
      CHECK(e.d2 <= 100);
    }
    if(e.status == 0x80 && e.at < 64 * sixteenth)
      CHECK(e.at % sixteenth == sixteenth / 4);
  }
  r.check_clean();

  Rig a;
  a.node.inputs.seed.value = 8;
  a.node.inputs.lock.value = 0.f;
  a.node.inputs.accent.value = true;
  a.node.inputs.velocity.value = {40.f, 120.f};
  a.run(64 * sixteenth);
  a.stop();
  int accents = 0;
  for(auto& e : a.ons())
  {
    if(e.d2 == 120)
      accents++;
    else
      CHECK(e.d2 <= 80);
  }
  CHECK(accents > 10);
  CHECK(accents < 54);
  a.check_clean();

  // Gate output: up at each note, down between them
  REQUIRE(a.gates.size() >= 2);
  CHECK(a.gates.front() == std::pair<int64_t, bool>{0, true});
  CHECK(a.gates[1] == std::pair<int64_t, bool>{sixteenth / 2, false});
}

TEST_CASE("Random sequencer: every note ends, whatever changes", "[midi_random_sequencer]")
{
  Rig r{100};
  r.node.inputs.seed.value = 99;
  r.node.inputs.lock.value = 0.f;
  r.node.inputs.gate_length.value = 2.f;
  r.node.inputs.octaves.value = 3;
  std::mt19937 g{1};
  Input in{{50000, libremidi::channel_events::control_change(1, 123, 0)},
           {90000, libremidi::channel_events::control_change(1, 120, 0)}};
  for(int i = 0; i < 300; i++)
  {
    r.run(r.pos + 3000 + int(g() % 5000), in);
    switch(g() % 7)
    {
      case 0:
        r.node.inputs.channel.value = 1 + int(g() % 3);
        break;
      case 1:
        r.node.inputs.length.value = 1 + int(g() % 16);
        break;
      case 2:
        r.node.inputs.gate_length.value = float(1 + int(g() % 200)) / 100.f;
        break;
      case 3:
        r.node.inputs.note_range.value = {float(g() % 60), float(60 + g() % 60)};
        break;
      case 4:
        r.node.inputs.engine.value = g() % 2 ? Seq::Turing : Seq::Marbles;
        break;
      case 5:
        r.set_rate_note(g() % 2 ? 1. / 32. : 1. / 8.);
        break;
      case 6:
        r.node.inputs.jitter.value = float(g() % 100) / 100.f;
        r.node.inputs.swing.value = float(g() % 100) / 100.f;
        break;
    }
    if(i == 100)
    {
      // Stop and play again
      r.stop();
      CHECK(r.node.sounding() == 0);
      r.rewind();
    }
  }
  r.stop();
  CHECK(r.node.sounding() == 0);
  r.check_clean();

  // All notes off ends what sounds at once, and passes through
  Rig c;
  c.node.inputs.gate_length.value = 1.f;
  c.run(sixteenth + 1000, {{sixteenth + 500, libremidi::channel_events::control_change(1, 123, 0)}});
  bool found = false;
  for(std::size_t i = 0; i < c.events.size(); i++)
  {
    if(c.events[i].status == 0xB0)
    {
      found = true;
      REQUIRE(i > 0);
      CHECK(c.events[i - 1].status == 0x80);
      CHECK(c.events[i - 1].at == sixteenth + 500);
    }
  }
  CHECK(found);
  CHECK(c.node.sounding() == 0);
  c.stop();
  c.check_clean();
}

TEST_CASE("Random sequencer: other messages pass through in order", "[midi_random_sequencer]")
{
  Rig r{100};
  r.node.inputs.seed.value = 2;
  Input in{
      {10, libremidi::channel_events::control_change(2, 7, 100)},
      {10, libremidi::channel_events::pitch_bend(2, 9000)},
      {5000, libremidi::channel_events::note_on(3, 60, 90)},
      {7000, libremidi::channel_events::note_off(3, 60, 0)},
      {7000, libremidi::channel_events::program_change(4, 12)}};
  r.run(2 * sixteenth, in);
  r.stop();
  std::vector<Event> other;
  for(auto& e : r.events)
    if(e.chan != 1)
      other.push_back(e);
  std::vector<Event> want{
      {10, 0xB0, 2, 7, 100},
      {10, 0xE0, 2, 9000 & 127, 9000 >> 7},
      {5000, 0x90, 3, 60, 90},
      {7000, 0x80, 3, 60, 0},
      {7000, 0xC0, 4, 12, 0}};
  CHECK(other == want);
}

TEST_CASE("Random sequencer: input notes and the Step input clock it", "[midi_random_sequencer]")
{
  Rig r{100};
  r.node.inputs.seed.value = 6;
  r.node.inputs.clock.value = Seq::clock_control::input_notes;
  Input in;
  for(int i = 0; i < 8; i++)
  {
    in.push_back({1000 + i * 7000, libremidi::channel_events::note_on(1, 30 + i, 100)});
    in.push_back({3000 + i * 7000, libremidi::channel_events::note_off(1, 30 + i, 0)});
  }
  in.push_back({60000, libremidi::channel_events::control_change(1, 1, 64)});
  r.run(70000, in);
  r.stop();
  const auto ons = r.ons();
  REQUIRE(ons.size() == 8);
  for(int i = 0; i < 8; i++)
  {
    CHECK(ons[i].at == 1000 + i * 7000);
    CHECK(ons[i].d1 >= 48);
    CHECK(ons[i].d1 <= 72);
  }
  int ccs = 0;
  for(auto& e : r.events)
    ccs += e.status == 0xB0;
  CHECK(ccs == 1);
  r.check_clean();

  Rig s{100};
  s.node.inputs.clock.value = Seq::clock_control::step_only;
  s.run(10000);
  CHECK(s.ons().empty());
  s.node.inputs.step.value.emplace();
  s.run(20000);
  s.node.inputs.step.value.emplace();
  s.run(30000);
  s.stop();
  REQUIRE(s.ons().size() == 2);
  CHECK(s.ons()[0].at == 10000);
  CHECK(s.ons()[1].at == 20000);
  s.check_clean();
}
