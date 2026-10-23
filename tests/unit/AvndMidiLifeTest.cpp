// Midi life (mtk::MidiLife): rule and pattern parsing, the generations, the
// play-head on the musical grid across buffer sizes and tempo changes, births,
// note maps, crowd velocity, reseeding, seeded takes; every note it starts it
// stops, what it does not use passes through, and operator() never allocates.

#include <Advanced/MidiScaler/MidiLife.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <map>
#include <new>
#include <random>
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
using Life = mtk::MidiLife;
constexpr double rate = 48000.;

struct Event
{
  int64_t at;
  int status;
  int chan;
  int d1;
  int d2;
  bool on() const { return status == 0x90; }
  bool off() const { return status == 0x80; }
  bool operator==(const Event&) const = default;
};

struct Rig
{
  Life node;
  int buffer;
  int64_t pos{};
  double quarters{};
  double tempo{120.};
  //! Where the current tempo started: positions are computed from it rather
  //! than accumulated, as the engine does
  int64_t anchor_pos{};
  double anchor_q{};
  std::vector<Event> events;
  std::vector<std::string> errors;
  std::map<std::pair<int, int>, int> sounding;
  std::vector<mtk::life::view_message> views;
  int allocations{};
  double note_value{1. / 16.}; //!< synced Rate, in whole notes; 0: free

  explicit Rig(int buffer = 512)
      : buffer{buffer}
  {
    node.prepare({.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = rate});
    node.inputs.midi.midi_messages.reserve(256);
    node.outputs.midi.midi_messages.reserve(4096);
    views.reserve(100000);
    node.send_message = [this](mtk::life::view_message m) {
      if(views.size() < views.capacity())
        views.push_back(m);
    };
    sync(1. / 16.);
  }

  //! A synced Rate, as the binding hands it: seconds at the current tempo.
  void sync(double whole)
  {
    note_value = whole;
    node.inputs.rate.sync = true;
    node.inputs.rate.value = float(4. * whole * 60. / tempo);
  }
  void free_rate(float seconds)
  {
    note_value = 0.;
    node.inputs.rate.sync = false;
    node.inputs.rate.value = seconds;
  }
  void set_tempo(double t)
  {
    anchor_pos = pos;
    anchor_q = quarters;
    tempo = t;
    if(note_value > 0.)
      sync(note_value);
  }

  //! A world that never changes: nothing is born, everything survives.
  void frozen(const char* pattern)
  {
    node.inputs.rule.value = Life::RuleCustom;
    node.inputs.custom_rule.value = "B/S012345678";
    node.inputs.start.value = Life::StartText;
    node.inputs.pattern.value = pattern;
  }

  using input = std::vector<std::pair<int64_t, libremidi::message>>;

  void tick(int frames, const input& in = {})
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

    const double end_q = anchor_q + double(pos + frames - anchor_pos) * tempo / 60. / rate;
    const double dq = end_q - quarters;
    halp::tick_musical tk{};
    tk.frames = frames;
    tk.position_in_frames = pos;
    tk.tempo = tempo;
    tk.signature = {4, 4};
    tk.start_position_in_quarters = quarters;
    tk.end_position_in_quarters = quarters + dq;
    tk.bar_at_start = std::floor(quarters / 4. + 1e-9) * 4.;
    tk.bar_at_end = std::floor((quarters + dq) / 4. + 1e-9) * 4.;

    g_allocations = 0;
    g_count_allocations = true;
    node(tk);
    g_count_allocations = false;
    allocations += g_allocations;

    int64_t last = 0;
    for(auto& m : node.outputs.midi.midi_messages)
    {
      if(frames > 0 && (m.timestamp < 0 || m.timestamp >= frames))
        errors.push_back("timestamp " + std::to_string(m.timestamp) + " outside the tick");
      if(frames == 0 && m.timestamp != 0)
        errors.push_back("timestamp in a 0-frame tick");
      if(m.timestamp < last)
        errors.push_back("out of order at " + std::to_string(pos + m.timestamp));
      last = m.timestamp;
      record(m, pos + m.timestamp);
    }
    pos += frames;
    quarters = end_q;
  }

  void record(const libremidi::message& m, int64_t at)
  {
    Event e{at, m.bytes[0] & 0xF0, (m.bytes[0] & 0x0F) + 1, m.size() > 1 ? m.bytes[1] : 0,
            m.size() > 2 ? m.bytes[2] : 0};
    if(e.status == 0x90 && e.d2 == 0)
      e.status = 0x80;
    events.push_back(e);
    if(!e.on() && !e.off())
      return;
    auto& n = sounding[{e.chan, e.d1}];
    if(e.on())
    {
      if(n > 0)
        errors.push_back("started twice: " + std::to_string(e.d1) + " at " + std::to_string(at));
      n++;
    }
    else if(n > 0)
      n--;
    else
      errors.push_back("stopped while silent: " + std::to_string(e.d1));
  }

  void run(int64_t until, const input& in = {})
  {
    while(pos < until)
      tick(int(std::min<int64_t>(buffer, until - pos)), in);
  }

  //! Moves the transport back, as a loop or a rewind does.
  void rewind(int64_t to = 0)
  {
    pos = to;
    quarters = double(to) * tempo / 60. / rate;
    anchor_pos = 0;
    anchor_q = 0.;
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
      if(e.on())
        r.push_back(e);
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
};

libremidi::message cc(int ctl, int value, int chan = 1)
{
  return libremidi::channel_events::control_change(chan, ctl, value);
}
}

TEST_CASE("Midi life parses Life-like rules", "[avnd][midi][life]")
{
  using mtk::life::parse_rule;
  auto life = parse_rule("B3/S23");
  REQUIRE(life);
  CHECK(life->birth == (1 << 3));
  CHECK(life->survive == ((1 << 2) | (1 << 3)));

  auto high = parse_rule("b36/s23");
  REQUIRE(high);
  CHECK(high->birth == ((1 << 3) | (1 << 6)));

  auto old = parse_rule("23/3");
  REQUIRE(old);
  CHECK(old->birth == (1 << 3));
  CHECK(old->survive == ((1 << 2) | (1 << 3)));

  auto seeds = parse_rule("B2/S");
  REQUIRE(seeds);
  CHECK(seeds->birth == (1 << 2));
  CHECK(seeds->survive == 0);

  auto compact = parse_rule("B3S23");
  REQUIRE(compact);
  CHECK(compact->survive == ((1 << 2) | (1 << 3)));

  CHECK(!parse_rule(""));
  CHECK(!parse_rule("B9/S23"));
  CHECK(!parse_rule("hello"));
  CHECK(!parse_rule("B3"));
}

TEST_CASE("Midi life parses drawn patterns and note maps", "[avnd][midi][life]")
{
  auto p = mtk::life::parse_pattern(".o./..o/ooo");
  CHECK(p.width == 3);
  CHECK(p.height == 3);
  CHECK(p.rows[0] == 0b010);
  CHECK(p.rows[1] == 0b100);
  CHECK(p.rows[2] == 0b111);

  auto q = mtk::life::parse_pattern("#.# \n.#.");
  CHECK(q.height == 2);
  CHECK(q.rows[0] == 0b101);
  CHECK(q.rows[1] == 0b010);

  mtk::life::note_map m;
  m.parse("36 38, 42 C4 F#2");
  CHECK(m.valid);
  REQUIRE(m.count == 5);
  CHECK(m.notes[0] == 36);
  CHECK(m.notes[3] == 60);
  CHECK(m.notes[4] == 42);
  m.parse("36 200");
  CHECK(!m.valid);
  CHECK(m.count == 0);
}

TEST_CASE("Midi life evolves blinkers, blocks and gliders", "[avnd][midi][life]")
{
  mtk::life::grid g{};
  const mtk::life::rule life{};
  // A horizontal blinker in the middle of an 8 x 8 grid
  g[4] = 0b0111000;
  auto g1 = Life::next_generation(g, 8, 8, false, life);
  CHECK(g1[3] == 0b0010000);
  CHECK(g1[4] == 0b0010000);
  CHECK(g1[5] == 0b0010000);
  CHECK(Life::next_generation(g1, 8, 8, false, life) == g);

  mtk::life::grid block{};
  block[2] = block[3] = 0b110;
  CHECK(Life::next_generation(block, 8, 8, false, life) == block);

  // On a torus a glider comes back where it started after 4 * width
  // generations of an 8 x 8 grid
  mtk::life::grid glider{};
  glider[7] = 0b010;
  glider[6] = 0b100;
  glider[5] = 0b111;
  auto w = glider;
  for(int i = 0; i < 32; i++)
    w = Life::next_generation(w, 8, 8, true, life);
  CHECK(w == glider);
  auto w4 = glider;
  for(int i = 0; i < 4; i++)
    w4 = Life::next_generation(w4, 8, 8, true, life);
  CHECK(w4 != glider);
  int pop = 0;
  for(auto r : w4)
    pop += std::popcount(r);
  CHECK(pop == 5);
}

TEST_CASE("Midi life steps on the sixteenth grid, whatever the buffer", "[avnd][midi][life]")
{
  std::vector<std::vector<Event>> takes;
  for(int buffer : {64, 100, 512, 4096})
  {
    Rig r{buffer};
    r.frozen("oooooooooooooooo");
    r.node.inputs.max_notes.value = 1;
    r.run(4 * 96000);
    r.stop();
    r.check_clean();
    const auto ons = r.ons();
    REQUIRE(ons.size() == 64);
    for(std::size_t i = 0; i < ons.size(); i++)
    {
      INFO("buffer " << buffer << " step " << i);
      CHECK(ons[i].at == int64_t(i) * 6000);
    }
    takes.push_back(r.events);
  }
  for(std::size_t i = 1; i < takes.size(); i++)
    CHECK(takes[i] == takes[0]);
}

TEST_CASE("Midi life stays on the beats across tempo changes", "[avnd][midi][life]")
{
  for(int buffer : {64, 100, 512})
  {
    Rig r{buffer};
    r.frozen("oooooooooooooooo");
    r.node.inputs.max_notes.value = 1;
    std::vector<std::pair<int64_t, double>> quarter_at; // frame -> quarter
    auto run = [&](int64_t until) {
      while(r.pos < until)
      {
        quarter_at.push_back({r.pos, r.quarters});
        r.tick(int(std::min<int64_t>(buffer, until - r.pos)));
      }
      quarter_at.push_back({r.pos, r.quarters});
    };
    run(48000);
    r.set_tempo(90.);
    run(150000);
    r.set_tempo(173.);
    run(250000);
    r.stop();
    r.check_clean();

    // Each note is on the frame where a sixteenth falls
    auto q_of = [&](int64_t f) {
      for(std::size_t i = 0; i + 1 < quarter_at.size(); i++)
        if(f >= quarter_at[i].first && f < quarter_at[i + 1].first)
        {
          const double span = quarter_at[i + 1].second - quarter_at[i].second;
          const double frames = double(quarter_at[i + 1].first - quarter_at[i].first);
          return quarter_at[i].second + (f - quarter_at[i].first) * span / frames;
        }
      return -1.;
    };
    const auto ons = r.ons();
    REQUIRE(ons.size() > 40);
    double last_q = -1.;
    for(auto& e : ons)
    {
      const double q = q_of(e.at);
      const double fq = q_of(e.at + 1) - q; // one frame, in quarters
      const double grid = std::ceil(q / 0.25 - 1e-9) * 0.25;
      INFO("buffer " << buffer << " at " << e.at << " q " << q);
      CHECK(grid - q < fq + 1e-9);
      if(last_q >= 0.)
        CHECK(std::abs(grid - last_q - 0.25) < 1e-6);
      last_q = grid;
    }
  }
}

TEST_CASE("Midi life free rate counts seconds from the start", "[avnd][midi][life]")
{
  Rig r{100};
  r.frozen("oooooooooooooooo");
  r.node.inputs.max_notes.value = 1;
  r.free_rate(0.05f);
  r.run(48000);
  r.stop();
  r.check_clean();
  const auto ons = r.ons();
  REQUIRE(ons.size() == 20);
  for(std::size_t i = 0; i < ons.size(); i++)
    CHECK(ons[i].at == int64_t(i) * 2400);
}

TEST_CASE("Midi life plays the rows of the play-head's column", "[avnd][midi][life]")
{
  Rig r;
  // Bottom row: every column. Row 2 from the bottom: column 1 only.
  r.frozen(".o.............. ................ oooooooooooooooo");
  r.node.inputs.height.value = 3;
  r.node.inputs.mode.value = 0; // major
  r.node.inputs.root.value = 2; // D
  r.node.inputs.octave.value = 3;
  r.node.inputs.max_notes.value = 4;
  r.node.inputs.gate.value = 50;
  r.run(2 * 6000);
  r.stop();
  r.check_clean();
  const auto ons = r.ons();
  REQUIRE(ons.size() == 3);
  CHECK(ons[0].at == 0);
  CHECK(ons[0].d1 == 50); // D3
  CHECK(ons[1].at == 6000);
  CHECK(ons[1].d1 == 50);
  CHECK(ons[2].at == 6000);
  CHECK(ons[2].d1 == 54); // two degrees up: F#3
  // Gate 50: half a step
  for(auto& e : r.events)
    if(e.off() && e.at < 12000)
      CHECK((e.at % 6000) == 3000);
}

TEST_CASE("Midi life note map plays drums on channel 10", "[avnd][midi][life]")
{
  Rig r;
  r.frozen("o... .o.. ..o. ...o");
  r.node.inputs.width.value = 4;
  r.node.inputs.height.value = 4;
  r.node.inputs.note_map.value = "36 38 42";
  r.node.inputs.channel.value = 10;
  r.run(4 * 6000);
  r.stop();
  r.check_clean();
  // The diagonal runs from the top left down to the bottom right: column 3
  // is the bottom row, column 0 the top one which has no note
  const auto ons = r.ons();
  REQUIRE(ons.size() == 3);
  CHECK(ons[0].at == 6000);
  CHECK(ons[0].d1 == 42);
  CHECK(ons[1].d1 == 38);
  CHECK(ons[2].at == 18000);
  CHECK(ons[2].d1 == 36);
  for(auto& e : ons)
    CHECK(e.chan == 10);
}

TEST_CASE("Midi life spreads Max notes over a crowded column", "[avnd][midi][life]")
{
  Rig r;
  r.frozen("o... o... o... o... o... o... o... o...");
  r.node.inputs.width.value = 4;
  r.node.inputs.mode.value = 13; // chromatic
  r.node.inputs.octave.value = 4;
  r.node.inputs.max_notes.value = 2;
  r.run(6000);
  r.stop();
  r.check_clean();
  const auto ons = r.ons();
  REQUIRE(ons.size() == 2);
  // The middle cells of the bottom and top halves: rows 2 and 6
  CHECK(ons[0].d1 == 62);
  CHECK(ons[1].d1 == 66);
}

TEST_CASE("Midi life velocity follows the neighbours", "[avnd][midi][life]")
{
  Rig r;
  // Column 0: an isolated cell (row 7) and a crowded one (row 1, with 4
  // neighbours)
  r.frozen("o... .... .... .... .... oo.. oo.. .o..");
  r.node.inputs.width.value = 4;
  r.node.inputs.wrap.value = false;
  r.node.inputs.mode.value = 13;
  r.node.inputs.octave.value = 4;
  r.node.inputs.velocity.value = 80;
  r.node.inputs.crowd.value = 10;
  r.node.inputs.max_notes.value = 8;
  r.run(6000);
  r.stop();
  r.check_clean();
  std::map<int, int> vel;
  for(auto& e : r.ons())
    vel[e.d1] = e.d2;
  REQUIRE(vel.count(60 + 7));
  REQUIRE(vel.count(60 + 1));
  CHECK(vel[60 + 7] == 60);  // no neighbour: 80 - 20
  CHECK(vel[60 + 1] == 100); // four neighbours: 80 + 20
  CHECK(vel[60 + 2] == 90);  // three neighbours
}

TEST_CASE("Midi life evolves once per sweep or every N steps", "[avnd][midi][life]")
{
  {
    Rig r;
    r.node.inputs.start.value = Life::StartBlinker;
    r.node.inputs.auto_reseed.value = false;
    r.run(16 * 6000);
    CHECK(r.node.generation() == 0);
    r.run(16 * 6000 + 1);
    CHECK(r.node.generation() == 1);
    r.run(64 * 6000 + 1);
    CHECK(r.node.generation() == 4);
    r.stop();
    r.check_clean();
  }
  {
    Rig r;
    r.node.inputs.start.value = Life::StartBlinker;
    r.node.inputs.auto_reseed.value = false;
    r.node.inputs.evolve.value = Life::EveryNSteps;
    r.node.inputs.steps_per_gen.value = 3;
    r.run(9 * 6000 + 1);
    CHECK(r.node.generation() == 3);
    r.stop();
    r.check_clean();
  }
}

TEST_CASE("Midi life plays only births when asked", "[avnd][midi][life]")
{
  Rig r;
  r.node.inputs.start.value = Life::StartBlinker;
  r.node.inputs.width.value = 8;
  r.node.inputs.height.value = 8;
  r.node.inputs.auto_reseed.value = false;
  r.node.inputs.play.value = Life::PlayBirths;
  r.node.inputs.mode.value = 13;
  r.node.inputs.octave.value = 4;
  r.node.inputs.max_notes.value = 8;
  r.node.inputs.evolve.value = Life::EveryNSteps;
  r.node.inputs.steps_per_gen.value = 8;
  // Generation 0, all born: the horizontal blinker on row 4 (from the
  // bottom, rows 0 to 7, the pattern in the centre: row 3 from the top).
  // Generation 1: the vertical one; its middle cell survived, two are born.
  r.run(16 * 6000);
  r.stop();
  r.check_clean();
  std::vector<std::pair<int64_t, int>> got;
  for(auto& e : r.ons())
    got.push_back({e.at / 6000, e.d1});
  const std::vector<std::pair<int64_t, int>> want{
      {2, 64}, {3, 64}, {4, 64}, {11, 63}, {11, 65}};
  CHECK(got == want);
}

TEST_CASE("Midi life reseeds a world that died or stalled", "[avnd][midi][life]")
{
  {
    Rig r;
    r.node.inputs.start.value = Life::StartText;
    r.node.inputs.pattern.value = "o"; // dies at once
    r.node.inputs.evolve.value = Life::EveryNSteps;
    r.node.inputs.steps_per_gen.value = 1;
    r.node.inputs.seed.value = 5;
    r.run(4 * 6000);
    CHECK(r.node.reseeds() >= 3);
    CHECK(r.node.population() == 1);
    r.stop();
    r.check_clean();
  }
  {
    Rig r;
    r.node.inputs.start.value = Life::StartText;
    r.node.inputs.pattern.value = "oo/oo"; // a block never changes
    r.node.inputs.evolve.value = Life::EveryNSteps;
    r.node.inputs.steps_per_gen.value = 1;
    r.node.inputs.patience.value = 5;
    r.run(5 * 6000 + 1);
    CHECK(r.node.reseeds() == 0);
    CHECK(r.node.generation() == 5);
    r.run(6 * 6000 + 1);
    CHECK(r.node.reseeds() == 1);
    CHECK(r.node.generation() == 0);
    r.stop();
    r.check_clean();
  }
  {
    Rig r;
    r.node.inputs.start.value = Life::StartText;
    r.node.inputs.pattern.value = "o";
    r.node.inputs.auto_reseed.value = false;
    r.run(32 * 6000);
    CHECK(r.node.reseeds() == 0);
    CHECK(r.node.population() == 0);
    r.stop();
    r.check_clean();
  }
}

TEST_CASE("Midi life reseeds on the impulse at the next step", "[avnd][midi][life]")
{
  Rig r;
  r.node.inputs.seed.value = 3;
  r.run(1000);
  r.node.inputs.reseed.value.emplace();
  r.tick(512);
  r.node.inputs.reseed.value.reset();
  CHECK(r.node.reseeds() == 0);
  r.run(6001);
  CHECK(r.node.reseeds() == 1);
  r.stop();
  r.check_clean();
}

TEST_CASE("Midi life seeded takes repeat, rewinds restart them", "[avnd][midi][life]")
{
  auto take = [](int seed, int buffer) {
    Rig r{buffer};
    r.node.inputs.seed.value = seed;
    r.node.inputs.evolve.value = Life::EveryNSteps;
    r.node.inputs.steps_per_gen.value = 2;
    r.run(8 * 96000);
    r.stop();
    r.check_clean();
    return r.events;
  };
  const auto a = take(42, 512);
  CHECK(a.size() > 50);
  CHECK(take(42, 100) == a);
  CHECK(take(43, 512) != a);
  // Free running: another world each time
  CHECK(take(0, 512) != take(0, 512));

  Rig r;
  r.node.inputs.seed.value = 42;
  r.node.inputs.evolve.value = Life::EveryNSteps;
  r.node.inputs.steps_per_gen.value = 2;
  r.run(4 * 96000);
  const auto first = r.ons();
  r.rewind();
  r.events.clear();
  r.run(4 * 96000);
  r.stop();
  r.check_clean();
  auto second = r.ons();
  CHECK(second == first);
}

TEST_CASE("Midi life ends every note it starts", "[avnd][midi][life]")
{
  std::mt19937 rng{7};
  for(int round = 0; round < 6; round++)
  {
    Rig r{std::uniform_int_distribution<int>{32, 700}(rng)};
    r.node.inputs.seed.value = 1 + round;
    r.node.inputs.density.value = 50;
    r.node.inputs.max_notes.value = 16;
    r.node.inputs.gate.value = 100;
    r.node.inputs.swing.value = 60;
    Rig::input in;
    for(int k = 1; k < 20; k++)
      in.push_back({k * 17011, cc(k % 2 ? 123 : 120, 0)});
    for(int k = 0; k < 400; k++)
    {
      switch(std::uniform_int_distribution<int>{0, 9}(rng))
      {
        case 0: r.node.inputs.channel.value = 1 + int(rng() % 16); break;
        case 1: r.node.inputs.octave.value = int(rng() % 8); break;
        case 2: r.node.inputs.width.value = 4 + int(rng() % 29); break;
        case 3: r.node.inputs.height.value = 2 + int(rng() % 15); break;
        case 4: r.sync(rng() % 2 ? 1. / 32. : 1. / 8.); break;
        case 5: r.node.inputs.gate.value = 5 + int(rng() % 96); break;
        case 6: r.node.inputs.rule.value = int(rng() % 6); break;
        case 7: r.node.inputs.note_map.value = rng() % 2 ? "36 36 38 36" : ""; break;
        case 8: r.node.inputs.play.value = int(rng() % 2); break;
        default: r.node.inputs.mode.value = int(rng() % 14); break;
      }
      r.run(r.pos + 2000, in);
      if(k % 97 == 50)
        r.stop();
      if(k % 131 == 70)
        r.rewind(r.pos / 2);
    }
    r.stop();
    INFO("round " << round);
    r.check_clean();
    CHECK(r.node.sounding() == 0);
    CHECK(r.ons().size() > 100);
  }
}

TEST_CASE("Midi life stop releases in a 0-frame tick", "[avnd][midi][life]")
{
  Rig r;
  r.frozen("oooooooooooooooo");
  r.node.inputs.gate.value = 100;
  r.run(1000);
  REQUIRE(r.node.sounding() == 1);
  const auto before = r.events.size();
  r.stop();
  REQUIRE(r.events.size() == before + 1);
  CHECK(r.events.back().off());
  CHECK(r.events.back().at == 1000);
  r.check_clean();
  // Playing again starts over from the first column, on the next sixteenth
  r.run(6001);
  CHECK(r.ons().back().at == 6000);
  CHECK(r.node.column() == 0);
  r.stop();
  r.check_clean();
}

TEST_CASE("Midi life passes through what it does not use", "[avnd][midi][life]")
{
  Rig r{100};
  r.frozen("o");
  r.node.inputs.width.value = 4;
  r.node.inputs.height.value = 2;
  Rig::input in{
      {10, libremidi::channel_events::note_on(3, 70, 99)},
      {10, cc(7, 100)},
      {20, libremidi::channel_events::pitch_bend(2, 300)},
      {30, libremidi::channel_events::note_off(3, 70, 12)},
      {6050, libremidi::channel_events::program_change(5, 9)}};
  r.run(7000, in);
  r.stop();
  std::vector<Event> through;
  for(auto& e : r.events)
    if(!(e.chan == 1 && (e.on() || e.off())))
      through.push_back(e);
  REQUIRE(through.size() == 5);
  CHECK(through[0] == Event{10, 0x90, 3, 70, 99});
  CHECK(through[1] == Event{10, 0xB0, 1, 7, 100});
  CHECK(through[2].status == 0xE0);
  CHECK(through[2].at == 20);
  CHECK(through[3] == Event{30, 0x80, 3, 70, 12});
  CHECK(through[4] == Event{6050, 0xC0, 5, 9, 0});
  CHECK(r.errors.empty());
}

TEST_CASE("Midi life tells the layer what to paint", "[avnd][midi][life]")
{
  Rig r;
  r.node.inputs.start.value = Life::StartGlider;
  r.run(3 * 6000 + 1);
  REQUIRE(!r.views.empty());
  const auto& v = r.views.back();
  CHECK(v.width == 16);
  CHECK(v.height == 8);
  CHECK(v.column == 3);
  CHECK(v.population == 5);
  CHECK(v.running);
  CHECK(v.rows == r.node.cells());

  // A click toggles a cell of the running world
  r.node.process_message(mtk::life::toggle_message{0, 0});
  CHECK(mtk::life::cell(r.node.cells(), 0, 0));
  r.tick(10);
  CHECK(mtk::life::cell(r.views.back().rows, 0, 0));

  mtk::life::life_view view;
  view.receive(r.views.back());
  CHECK(view.has_state);
  CHECK(view.born(0, 0)); // generation 0: every cell is new
  r.stop();
  r.check_clean();
  CHECK(!r.views.back().running);
  CHECK(r.views.back().column == -1);
}

TEST_CASE("Midi life view highlights births", "[avnd][midi][life]")
{
  mtk::life::life_view view;
  mtk::life::view_message m{};
  m.width = 8;
  m.height = 8;
  m.generation = 3;
  m.rows[4] = 0b0111000;
  view.receive(m);
  m.generation = 4;
  m.rows = {};
  m.rows[3] = m.rows[4] = m.rows[5] = 0b0010000;
  view.receive(m);
  CHECK(view.born(4, 3));
  CHECK(view.born(4, 5));
  CHECK(!view.born(4, 4));
  // A skipped generation: nothing is known to be new
  m.generation = 6;
  view.receive(m);
  CHECK(!view.born(4, 3));
}
