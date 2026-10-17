// Patternist under stress: the audio thread's real-time safety, notes that
// must never be left sounding, hostile documents and pastes, undo, and the cost
// of painting. The [.][measure] cases print numbers and the [.][shots] ones
// render images into $PATTERNIST_SHOTS; both run on demand only.

#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <score/model/Skin.hpp>

#include <ossia/detail/flicks.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QMimeData>
#include <QPainter>

#include <Patternist/Commands/PatternProperties.hpp>
#include <Patternist/PatternEdit.hpp>
#include <Patternist/PatternEditor.hpp>
#include <Patternist/PatternModel.hpp>
#include <Patternist/PatternNode.hpp>
#include <Patternist/PatternView.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <thread>
#include <vector>


// ---------------------------------------------------------------------------
// Heap allocation counter, armed only on the thread that asks for it.
namespace
{
thread_local bool g_counting = false;
thread_local long g_allocations = 0;

void* counted_alloc(std::size_t n)
{
  if(g_counting)
    ++g_allocations;
  return std::malloc(n ? n : 1);
}

struct alloc_scope
{
  alloc_scope()
  {
    g_allocations = 0;
    g_counting = true;
  }
  ~alloc_scope() { g_counting = false; }
  long count() const { return g_allocations; }
};
}

void* operator new(std::size_t n)
{
  if(void* p = counted_alloc(n))
    return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n)
{
  if(void* p = counted_alloc(n))
    return p;
  throw std::bad_alloc{};
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
  return counted_alloc(n);
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
  return counted_alloc(n);
}
void operator delete(void* p) noexcept
{
  std::free(p);
}
void operator delete[](void* p) noexcept
{
  std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
  std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
  std::free(p);
}
void operator delete(void* p, const std::nothrow_t&) noexcept
{
  std::free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
  std::free(p);
}

namespace
{
using namespace Patternist;

constexpr int note_on = CMIDI2_STATUS_NOTE_ON;
constexpr int note_off = CMIDI2_STATUS_NOTE_OFF;

struct decoded
{
  int status{};
  int note{};
  int64_t timestamp{};
};

Lane lane(uint8_t note, std::vector<Note> steps)
{
  Lane l{std::move(steps), note};
  l.params.resize(l.pattern.size());
  return l;
}

//! What a receiving synth hears: the notes sounding, a note-on of a sounding
//! note counted as a second voice.
struct synth
{
  std::map<int, int> voices;
  int double_ons{};
  int64_t ons{};
  int64_t offs{};

  void hear(const std::vector<decoded>& msgs)
  {
    for(auto& m : msgs)
    {
      if(m.status == note_on)
      {
        ons++;
        if(voices[m.note] > 0)
          double_ons++;
        voices[m.note]++;
      }
      else if(m.status == note_off)
      {
        offs++;
        if(voices[m.note] > 0)
          voices[m.note]--;
      }
    }
  }
  std::set<int> sounding() const
  {
    std::set<int> s;
    for(auto [n, c] : voices)
      if(c > 0)
        s.insert(n);
    return s;
  }
  int voice_count() const
  {
    int c = 0;
    for(auto [n, v] : voices)
      c += v;
    return c;
  }
};

struct fixture
{
  pattern_node node;
  ossia::execution_state st; // modelToSamplesRatio == 1
  ossia::midi_port& port{*node.out.target<ossia::midi_port>()};

  fixture()
  {
    node.channel = to_midi_channel(1);
    node.in_flight_channel = node.channel;
    port.messages.reserve(8192);
  }

  Pattern& set(std::vector<Lane> lanes, int length, int division = 16)
  {
    Pattern p;
    p.lanes = std::move(lanes);
    p.length = length;
    p.division = division;
    node.patterns = {std::move(p)};
    node.current_pattern = 0;
    return node.patterns[0];
  }

  std::vector<decoded> collect()
  {
    std::vector<decoded> out;
    for(const auto& m : port.messages)
      out.push_back(
          {cmidi2_ump_get_status_code(m.data), cmidi2_ump_get_midi2_note_note(m.data),
           m.timestamp});
    return out;
  }

  //! [prev; date[ in model units, 1000 per quarter, with its musical positions.
  ossia::token_request musical(int64_t prev, int64_t date)
  {
    ossia::token_request tk{
        ossia::time_value{prev},
        ossia::time_value{date},
        ossia::time_value{100000000},
        ossia::time_value{0},
        date >= prev ? 1. : -1.,
        ossia::time_signature{4, 4},
        120.};
    tk.musical_start_position = prev / 1000.;
    tk.musical_end_position = date / 1000.;
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
    return tk;
  }

  void run(const ossia::token_request& tk)
  {
    port.messages.clear();
    ossia::exec_state_facade fac{&st};
    static_cast<ossia::graph_node&>(node).run(tk, fac);
  }

  std::vector<decoded> tick(int64_t prev, int64_t date)
  {
    run(musical(prev, date));
    return collect();
  }

  //! A tick without musical time: one step per tick, played at once.
  std::vector<decoded> untimed()
  {
    ossia::token_request tk{
        ossia::time_value{0},
        ossia::time_value{100},
        ossia::time_value{100000},
        ossia::time_value{0},
        1.,
        ossia::time_signature{4, 4},
        120.};
    run(tk);
    return collect();
  }

  std::vector<decoded> stop()
  {
    node.all_notes_off();
    run(ossia::token_request{});
    return collect();
  }
};
}

// ===========================================================================
// 1. Real-time safety of pattern_node::run
// ===========================================================================

TEST_CASE(
    "patternist: a timed tick does not allocate",
    "[midi][pattern][rt]")
{
  fixture f;
  auto& p = f.set(
      {lane(36, {Note::Note, Note::Rest, Note::Note, Note::Rest}),
       lane(38, {Note::Rest, Note::Note, Note::Rest, Note::Note})},
      4);
  for(auto& l : p.lanes)
    for(auto& s : l.params)
      s.gate = 0.5f;

  // Warm up: every buffer that can grow has grown.
  int64_t t = 0;
  for(int i = 0; i < 64; i++, t += 250)
    f.tick(t, t + 250);

  long allocs = 0;
  {
    alloc_scope a;
    for(int i = 0; i < 64; i++, t += 250)
    {
      f.port.messages.clear();
      ossia::exec_state_facade fac{&f.st};
      static_cast<ossia::graph_node&>(f.node).run(f.musical(t, t + 250), fac);
    }
    allocs = a.count();
  }
  UNSCOPED_INFO("heap allocations in 64 steady-state ticks: " << allocs);
  CHECK(allocs == 0);
}

TEST_CASE(
    "patternist: an untimed tick does not allocate",
    "[midi][pattern][rt]")
{
  fixture f;
  f.set(
      {lane(36, {Note::Note, Note::Rest}), lane(38, {Note::Rest, Note::Note})}, 2);
  for(int i = 0; i < 16; i++)
    f.untimed();

  long allocs = 0;
  {
    alloc_scope a;
    for(int i = 0; i < 16; i++)
    {
      f.port.messages.clear();
      ossia::exec_state_facade fac{&f.st};
      ossia::token_request tk{
          ossia::time_value{0}, ossia::time_value{100}, ossia::time_value{100000},
          ossia::time_value{0}, 1., ossia::time_signature{4, 4}, 120.};
      static_cast<ossia::graph_node&>(f.node).run(tk, fac);
    }
    allocs = a.count();
  }
  UNSCOPED_INFO("heap allocations in 16 untimed ticks: " << allocs);
  CHECK(allocs == 0);
}

TEST_CASE(
    "patternist: 64 notes starting together do not allocate",
    "[midi][pattern][rt]")
{
  fixture f;
  std::vector<Lane> lanes;
  for(int i = 0; i < 64; i++)
    lanes.push_back(lane(uint8_t(i), {Note::Note, Note::Legato}));
  f.set(std::move(lanes), 2);

  long allocs = 0;
  {
    alloc_scope a;
    ossia::exec_state_facade fac{&f.st};
    f.port.messages.clear();
    static_cast<ossia::graph_node&>(f.node).run(f.musical(0, 250), fac);
    allocs = a.count();
  }
  UNSCOPED_INFO("allocations while 64 notes start: " << allocs);
  CHECK(allocs == 0);
  int held = 0;
  for(auto c : f.node.in_flight)
    held += c > 0;
  CHECK(held == 64);
}

TEST_CASE(
    "patternist: walking 16 grid points of a tick does not allocate",
    "[midi][pattern][rt]")
{
  fixture f;
  auto tk = f.musical(0, 1000); // one quarter: 16 points at 1/64
  long allocs = 0;
  int points = 0;
  {
    alloc_scope a;
    tk.for_each_quantification_date(64., [&](const auto&) { points++; });
    allocs = a.count();
  }
  CHECK(points == 16);
  UNSCOPED_INFO("allocations walking the points: " << allocs);
  CHECK(allocs == 0);
}

// ===========================================================================
// 2. Stuck and lost notes
// ===========================================================================

TEST_CASE(
    "patternist: an early step landing on a late one still releases it",
    "[midi][pattern][notes]")
{
  // Step 0 late by half a step, step 1 early by half: both land on the same
  // position. Step 1 releases what step 0 holds, but the release sorts before
  // the note-on it releases, finds nothing in flight and is dropped. Step 0
  // plays its first pass only, so nothing ever releases note 36 again.
  fixture f;
  auto& p = f.set({lane(36, {Note::Note, Note::Note})}, 2);
  auto& l = p.lanes[0];
  l.params[0].offset = 0.5f;
  l.params[0].condition = Condition::First;
  l.params[1].offset = -0.5f;
  l.params[1].pitch = 1;

  synth s;
  int64_t t = 0;
  for(int i = 0; i < 32; i++, t += 250)
    s.hear(f.tick(t, t + 250));

  const auto sounding = s.sounding();
  std::string str;
  for(int n : sounding)
    str += std::to_string(n) + " ";
  UNSCOPED_INFO("sounding after 32 steps of a one-voice lane: " << str);
  CHECK(sounding.count(36) == 0);
  CHECK(sounding.size() <= 1);
}

TEST_CASE(
    "patternist: two lanes on one note share a voice",
    "[midi][pattern][notes]")
{
  // Lane A holds 36 for the whole step, lane B strikes 36 short on the same step.
  fixture f;
  auto& p = f.set(
      {lane(36, {Note::Note, Note::Rest}), lane(36, {Note::Note, Note::Rest})}, 2);
  p.lanes[1].params[0].gate = 0.5f;

  synth s;
  int64_t t = 0;
  for(int i = 0; i < 8; i++, t += 250)
    s.hear(f.tick(t, t + 250));
  s.hear(f.stop());

  UNSCOPED_INFO(
      "note-ons over a sounding note: " << s.double_ons << ", ons " << s.ons << ", offs "
                                         << s.offs << ", voices left after stop "
                                         << s.voice_count());
  CHECK(s.double_ons == 0);
  CHECK(s.voice_count() == 0);
}

TEST_CASE(
    "patternist: a step of 48 lanes of 12 hits drops no note",
    "[midi][pattern][notes]")
{
  // 48 lanes, each striking a 12-hit ratchet (6 x the Ratchet input's 2) on
  // step 0 and resting after: 48 * 24 = 1152 events for a single step, well
  // within max_lanes (128) * max_ratchet (16).
  fixture f;
  std::vector<Lane> lanes;
  for(int i = 0; i < 48; i++)
  {
    auto l = lane(uint8_t(20 + i), {Note::Note, Note::Rest, Note::Rest, Note::Rest});
    l.params[0].ratchet = 6;
    l.params[0].gate = 0.5f;
    lanes.push_back(std::move(l));
  }
  f.set(std::move(lanes), 4);
  f.node.ratchet_scale = 2.f;

  synth s;
  int64_t t = 0;
  std::set<int> struck;
  for(int i = 0; i < 4; i++, t += 250)
  {
    auto m = f.tick(t, t + 250);
    for(auto& e : m)
      if(e.status == note_on)
        struck.insert(e.note);
    s.hear(m);
  }

  UNSCOPED_INFO(
      "lanes heard: " << struck.size() << " of 48; ons " << s.ons << " (expected "
                      << 48 * 12 << "), notes still sounding after the rests: "
                      << s.sounding().size());
  CHECK(struck.size() == 48);
  CHECK(s.sounding().empty());
}

TEST_CASE(
    "patternist: removing a lane releases what it held",
    "[midi][pattern][notes]")
{
  fixture f;
  f.set(
      {lane(36, {Note::Note, Note::Rest, Note::Rest, Note::Rest}),
       lane(38, {Note::Note, Note::Rest, Note::Rest, Note::Rest})},
      4);

  synth s;
  s.hear(f.tick(0, 250)); // both lanes hold their note (gate 1)
  REQUIRE(s.sounding() == std::set<int>{36, 38});

  // What the executor's patternsChanged handler does once the lane is removed.
  {
    Pattern p = f.node.patterns[0];
    removeLane(p, 1);
    std::vector<Pattern> list{p};
    std::swap(f.node.patterns, list);
  }

  int64_t t = 250;
  for(int i = 0; i < 16; i++, t += 250)
    s.hear(f.tick(t, t + 250));

  std::string str;
  for(int n : s.sounding())
    str += std::to_string(n) + " ";
  UNSCOPED_INFO("sounding 16 steps after the removal (36 is lane 0's current step): " << str);
  CHECK(s.sounding().count(38) == 0);
}

TEST_CASE(
    "patternist: a NaN on the Swing or Pace input is ignored",
    "[midi][pattern][notes]")
{
  const auto run = [](auto setup) {
    fixture f;
    auto& p = f.set(
        {lane(36, {Note::Note, Note::Note, Note::Note, Note::Note})}, 4);
    for(auto& s : p.lanes[0].params)
      s.gate = 0.5f;
    setup(f);
    synth s;
    int64_t t = 0;
    for(int i = 0; i < 2048; i++, t += 250)
      s.hear(f.tick(t, t + 250));
    return std::make_tuple(s.ons, f.node.queue_size, s.sounding().size());
  };

  const auto [ons_ok, q_ok, _] = run([](fixture&) {});
  const auto [ons_swing, q_swing, stuck_swing] = run([](fixture& f) {
    f.node.swing_in->write_value(std::nanf(""), 0);
  });
  const auto [ons_pace, q_pace, stuck_pace] = run([](fixture& f) {
    f.node.pace_in->write_value(std::nanf(""), 0);
  });
  std::printf(
      "[measure] 2048 steps: note-ons %lld (queue %d) clean, %lld (queue %d) with NaN "
      "swing, %lld (queue %d) with NaN pace\n",
      (long long)ons_ok, q_ok, (long long)ons_swing, q_swing, (long long)ons_pace,
      q_pace);
  CHECK(ons_swing == ons_ok);
  CHECK(ons_pace == ons_ok);
  CHECK(q_swing < pattern_node::max_events);
}

TEST_CASE(
    "patternist: a NaN step parameter is reset when pasted",
    "[midi][pattern][load]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // score's rapidjson accepts NaN (kParseNanAndInfFlag).
    QMimeData md;
    md.setText(QStringLiteral(
        R"({"Length":4,"Division":16,"Lanes":[{"Note":36,"Pattern":"1111",)"
        R"("Steps":[{"Step":0,"Offset":NaN,"Gate":NaN,"Velocity":NaN}]}]})"));
    auto p = patternFromMime(md);
    REQUIRE(p);
    const auto& s = p->lanes[0].params[0];
    UNSCOPED_INFO("offset " << s.offset << " gate " << s.gate << " velocity " << s.velocity);
    CHECK(std::isfinite(s.offset));
    CHECK(std::isfinite(s.gate));

    // And what it does once played: the step's events can never come due.
    fixture f;
    f.node.patterns = {*p};
    synth syn;
    int64_t t = 0;
    for(int i = 0; i < 2048; i++, t += 250)
      syn.hear(f.tick(t, t + 250));
    UNSCOPED_INFO(
        "queue " << f.node.queue_size << " / " << pattern_node::max_events << ", ons "
                 << syn.ons);
    CHECK(f.node.queue_size < pattern_node::max_events);
  });
}

TEST_CASE("patternist: a pattern holds at most the lanes the engine plays", "[midi][pattern][limits]")
{
  Pattern p;
  p.lanes.push_back(Lane{std::vector<Note>(16, Note::Rest), 36});
  normalize(p);
  for(int i = 0; i < 140; i++)
    addLane(p);
  UNSCOPED_INFO("addLane let the pattern grow to " << p.lanes.size() << " lanes");
  CHECK(int(p.lanes.size()) <= pattern_node::max_lanes);

  fixture f;
  p.lanes.back().note = 99;
  p.lanes.back().pattern[0] = Note::Note;
  f.node.patterns = {p};
  bool heard = false;
  for(auto& m : f.tick(0, 250))
    heard |= m.status == note_on && m.note == 99;
  CHECK(heard);
}

TEST_CASE(
    "patternist: a transport jump moves the pattern with the playhead",
    "[midi][pattern][notes]")
{
  // The layer draws step 0 at the first grid point from the process start
  // (timelineSlots), so after jumping back to the start the engine should
  // play step 0 again.
  auto node = std::make_shared<pattern_node>();
  pattern_node_process proc{node};
  fixture f; // for its execution state
  auto& port = *node->out.target<ossia::midi_port>();
  Pattern p;
  p.length = 4;
  p.division = 16;
  p.lanes = {
      lane(36, {Note::Note, Note::Rest, Note::Rest, Note::Rest}),
      lane(37, {Note::Rest, Note::Note, Note::Rest, Note::Rest}),
      lane(38, {Note::Rest, Note::Rest, Note::Note, Note::Rest}),
      lane(39, {Note::Rest, Note::Rest, Note::Rest, Note::Note})};
  node->patterns = {p};

  const auto struck = [&](int64_t a, int64_t b) {
    port.messages.clear();
    ossia::exec_state_facade fac{&f.st};
    static_cast<ossia::graph_node&>(*node).run(f.musical(a, b), fac);
    int n = -1;
    for(auto& m : port.messages)
      if(cmidi2_ump_get_status_code(m.data) == note_on)
        n = cmidi2_ump_get_midi2_note_note(m.data);
    return n;
  };
  CHECK(struck(0, 250) == 36);
  CHECK(struck(250, 500) == 37);
  CHECK(struck(500, 750) == 38);

  // The user clicks back to the start of the process.
  proc.transport(ossia::time_value{0});
  const int after = struck(0, 250);
  UNSCOPED_INFO("note struck at the start after jumping back: " << after);
  CHECK(after == 36);
}

TEST_CASE(
    "patternist: controls are held to their range, and NaN ignored",
    "[midi][pattern][notes]")
{
  // The inlets and the GUI side (the executor) both go through these.
  pattern_node n;
  n.set_swing(6.f);
  CHECK(n.swing == 1.f);
  n.set_swing(std::nanf(""));
  CHECK(n.swing == 1.f);
  n.set_density(-1.f);
  CHECK(n.density == 0.f);
  n.set_velocity_scale(std::numeric_limits<float>::infinity());
  CHECK(n.velocity_scale == 1.f);
  n.set_ratchet_scale(100.f);
  CHECK(n.ratchet_scale == 8.f);
  n.set_pace_offset(-std::numeric_limits<float>::infinity());
  CHECK(n.pace_offset == 0.f);
  n.set_repeat_time({std::nanf(""), 1.f});
  CHECK(n.repeat_time_value[0] == 0.f);
  n.set_switch_rate(std::nanf(""));
  CHECK(n.switch_rate == 1.);

  // What a swing of 6 used to do: notes left sounding.
  fixture f;
  f.set({lane(36, {Note::Note, Note::Note}), lane(38, {Note::Note, Note::Note})}, 2);
  for(auto& l : f.node.patterns[0].lanes)
    l.params[1].pitch = 1;
  f.node.set_swing(6.f);
  synth s;
  int64_t t = 0;
  for(int i = 0; i < 64; i++, t += 250)
    s.hear(f.tick(t, t + 250));
  CHECK(s.sounding().size() <= 2);
  CHECK(s.double_ons == 0);
}

// ===========================================================================
// 3. Cost of the engine
// ===========================================================================

TEST_CASE("patternist: worst-case run() cost", "[.][midi][pattern][measure]")
{
  // 128 lanes x 128 steps, every step a note with ratchet 8, Ratchet input 8,
  // pace bent, gate 0.5.
  fixture f;
  std::vector<Lane> lanes;
  for(int i = 0; i < 128; i++)
  {
    auto l = lane(uint8_t(i), std::vector<Note>(128, Note::Note));
    for(auto& s : l.params)
    {
      s.ratchet = 8;
      s.gate = 0.5f;
      s.pace = 0.5f;
      s.offset = 0.1f;
    }
    lanes.push_back(std::move(l));
  }
  f.set(std::move(lanes), 128);
  f.node.ratchet_scale = 8.f;

  using clk = std::chrono::steady_clock;
  const auto measure = [&](int64_t span, int n) {
    int64_t t = 0;
    double worst = 0, total = 0;
    for(int i = 0; i < n; i++, t += span)
    {
      auto t0 = clk::now();
      f.port.messages.clear();
      ossia::exec_state_facade fac{&f.st};
      static_cast<ossia::graph_node&>(f.node).run(f.musical(t, t + span), fac);
      const double us = std::chrono::duration<double, std::micro>(clk::now() - t0).count();
      worst = std::max(worst, us);
      total += us;
    }
    return std::make_pair(total / n, worst);
  };
  const auto [avg1, worst1] = measure(250, 512);
  const auto [avg4, worst4] = measure(1000, 512);
  std::printf(
      "[measure] worst-case pattern: one step per tick avg %.1f us worst %.1f us; four "
      "steps per tick avg %.1f us worst %.1f us; queue %d\n",
      avg1, worst1, avg4, worst4, f.node.queue_size);
  SUCCEED();
}

TEST_CASE(
    "patternist: a division read from a file is held to 1/64",
    "[midi][pattern][perf][load]")
{
  Pattern p;
  p.length = 16;
  p.division = 16;
  for(int i = 0; i < 128; i++)
  {
    Lane l{std::vector<Note>(16, Note::Note), uint8_t(i)};
    l.division = 1 << 24;
    p.lanes.push_back(std::move(l));
  }
  p.division = 1 << 24;
  normalize(p);
  CHECK(p.division == Pattern::maxDivision);
  CHECK(p.lanes[0].division == Pattern::maxDivision);

  fixture f;
  f.node.patterns = {p};
  using clk = std::chrono::steady_clock;
  int64_t t = 0;
  double total = 0;
  long allocs = 0;
  for(int i = 0; i < 32; i++, t += 250)
  {
    auto t0 = clk::now();
    alloc_scope a;
    f.port.messages.clear();
    ossia::exec_state_facade fac{&f.st};
    static_cast<ossia::graph_node&>(f.node).run(f.musical(t, t + 250), fac);
    allocs += a.count();
    total += std::chrono::duration<double, std::micro>(clk::now() - t0).count();
  }
  std::printf(
      "[measure] 128 lanes at 1/64: %.1f us per tick, %ld allocations in 32 ticks\n",
      total / 32, allocs);
  CHECK(allocs == 0);
}

// ===========================================================================
// 4. The data the GUI reads from the engine
// ===========================================================================

TEST_CASE(
    "patternist: the GUI reads what the engine publishes, never the pattern list",
    "[midi][pattern][race]")
{
  fixture f;
  std::vector<Pattern> four(1);
  four[0].length = 4;
  four[0].lanes = {
      lane(36, {Note::Note, Note::Rest, Note::Rest, Note::Rest}),
      lane(37, {Note::Rest, Note::Note, Note::Rest, Note::Rest}),
      lane(38, {Note::Rest, Note::Rest, Note::Note, Note::Rest}),
      lane(39, {Note::Rest, Note::Rest, Note::Rest, Note::Note})};
  auto one = four;
  one[0].lanes.resize(1);

  f.node.patterns = four;
  f.tick(0, 250);
  CHECK(f.node.shown.lane_count.load() == 4);
  CHECK(f.node.shown.pattern.load() == 0);
  CHECK(f.node.shown.lanes[0].load() == 0);

  // The executor swaps lists and runs ticks on the audio thread while its
  // timer reads on the GUI thread: what it reads is always some tick's.
  std::atomic<bool> done{false};
  std::thread audio([&] {
    int64_t t = 250;
    auto other = one;
    for(int i = 0; i < 20000; i++, t += 250)
    {
      std::swap(f.node.patterns, other);
      f.tick(t, t + 250);
    }
    done = true;
  });
  long bad = 0, reads = 0;
  while(!done.load())
  {
    const int count = f.node.shown.lane_count.load(std::memory_order_relaxed);
    const int pat = f.node.shown.pattern.load(std::memory_order_relaxed);
    reads++;
    if((count != 1 && count != 4) || pat != 0)
      bad++;
  }
  audio.join();
  UNSCOPED_INFO(bad << " inconsistent reads of " << reads);
  CHECK(bad == 0);
}

// ===========================================================================
// 5. Model, loading and commands
// ===========================================================================

namespace
{
Patternist::ProcessModel* add_pattern_process(score::Document& doc)
{
  auto& interval = score::test::base_interval(doc);
  auto* cmd = new Scenario::Command::AddOnlyProcessToInterval{
      interval, Metadata<ConcreteKey_k, Patternist::ProcessModel>::get(), QString{},
      QPointF{}};
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);
  auto it = interval.processes.find(cmd->processId());
  return it != interval.processes.end()
             ? dynamic_cast<Patternist::ProcessModel*>(&*it)
             : nullptr;
}
}

TEST_CASE(
    "patternist: a loaded document's selection and channel are held to range",
    "[midi][pattern][load]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Patternist::ProcessModel proc{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    const Process::ProcessModel& as_base = proc;

    JSONReader reader;
    reader.readFrom(as_base);
    rapidjson::Document jdoc = toValue(reader);
    jdoc["Pattern"].SetInt(7);
    jdoc["Channel"].SetInt(99);

    JSONWriter writer{jdoc};
    auto clone = dynamic_cast<Patternist::ProcessModel*>(
        deserialize_interface(pl, writer, doc->context(), owner));
    REQUIRE(clone);
    UNSCOPED_INFO(
        "current " << clone->currentPattern() << " of " << clone->patterns().size()
                   << " patterns, channel " << clone->channel());
    CHECK(clone->currentPattern() < int(clone->patterns().size()));
    CHECK(clone->channel() <= 16);
    delete clone;
  });
}

TEST_CASE(
    "patternist: undoing a selection past the end gives the list back",
    "[midi][pattern][undo]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Patternist::ProcessModel proc{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr};
    const auto before = proc.patterns();
    // A control-value command and its undo: the port goes to 5 and back to 0.
    proc.patternSelect->setValue(5);
    proc.patternSelect->setValue(0);
    UNSCOPED_INFO(
        "patterns before: " << before.size() << ", after the edit and its undo: "
                            << proc.patterns().size());
    CHECK(proc.patterns() == before);
  });
}

TEST_CASE(
    "patternist: an ongoing edit follows the pattern it is made on",
    "[midi][pattern][undo]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto proc = add_pattern_process(*doc);
    REQUIRE(proc);

    auto list = proc->patterns();
    list.push_back(list[0]);
    list[1].lanes[0].note = 50;
    proc->setPatterns(list);
    const Pattern original0 = proc->patterns()[0];

    // A paint drag: its first step is on pattern 0...
    auto& disp = doc->context().dispatcher;
    auto p0 = proc->patterns()[0];
    p0.lanes[0].pattern[0] = Note::Note;
    disp.submit<UpdatePattern>(*proc, 0, p0);

    // ...then the Pattern port, automated, switches the shown pattern, and the
    // drag continues on pattern 1, as Controller::paintStep submits it.
    proc->setCurrentPattern(1);
    auto p1 = proc->patterns()[1];
    p1.lanes[0].pattern[1] = Note::Note;
    disp.submit<UpdatePattern>(*proc, 1, p1);
    disp.commit();

    UNSCOPED_INFO(
        "pattern 0 lane note now " << int(proc->patterns()[0].lanes[0].note)
                                   << ", pattern 1 lane note "
                                   << int(proc->patterns()[1].lanes[0].note));
    CHECK(proc->patterns()[0].lanes[0].note == original0.lanes[0].note);
    CHECK(proc->patterns()[1] == p1);
  });
}

// ===========================================================================
// 6. Painting
// ===========================================================================

namespace
{
Pattern big_pattern(int lanes, int steps)
{
  Pattern p;
  p.length = steps;
  p.division = 16;
  for(int i = 0; i < lanes; i++)
  {
    Lane l{std::vector<Note>(steps, Note::Note), uint8_t(i % 128)};
    l.params.resize(steps);
    for(int s = 0; s < steps; s++)
      l.params[s].ratchet = 1 + s % 8;
    p.lanes.push_back(std::move(l));
  }
  return p;
}

double paint_ms(Patternist::View& v, int n, QSize sz)
{
  QImage img{sz, QImage::Format_ARGB32_Premultiplied};
  QElapsedTimer t;
  t.start();
  for(int i = 0; i < n; i++)
  {
    QPainter painter{&img};
    static_cast<QGraphicsItem&>(v).paint(&painter, nullptr, nullptr);
  }
  return t.nsecsElapsed() / 1e6 / n;
}
}

TEST_CASE("patternist: the editor paints in linear time in the lanes", "[.][midi][pattern][measure]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QSize sz{1600, 900};
    std::map<int, double> ms;
    for(int lanes : {16, 32, 64, 128})
    {
      Patternist::ProcessModel proc{
          TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr};
      proc.setPattern(0, big_pattern(lanes, 16));
      Patternist::View v{proc, nullptr};
      v.setMode(Patternist::View::Mode::Fit);
      v.setAdvanced(true);
      v.setWidth(sz.width());
      v.setHeight(sz.height());
      paint_ms(v, 3, sz);
      ms[lanes] = paint_ms(v, 20, sz);
    }
    // Same cell count throughout: lanes x steps grows, but at 16 steps the cells
    // are cheap next to the header, whose every button measures every lane name.
    std::printf(
        "[measure] editor paint, 16 steps: 16 lanes %.2f ms, 32 lanes %.2f ms, 64 "
        "lanes %.2f ms, 128 lanes %.2f ms\n",
        ms[16], ms[32], ms[64], ms[128]);

    Patternist::ProcessModel proc{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr};
    proc.setPattern(0, big_pattern(128, 128));
    Patternist::View v{proc, nullptr};
    v.setMode(Patternist::View::Mode::Fit);
    v.setAdvanced(true);
    v.setWidth(sz.width());
    v.setHeight(sz.height());
    paint_ms(v, 2, sz);
    std::printf("[measure] editor paint, 128 x 128: %.2f ms\n", paint_ms(v, 10, sz));
    // Doubling the lanes should at most double the time.
    CHECK(ms[128] < 8.5 * ms[16]);
  });
}

TEST_CASE("patternist: renders of the layer and the editor grid", "[.][midi][pattern][shots]")
{
  const QString dir = qEnvironmentVariable("PATTERNIST_SHOTS");
  if(dir.isEmpty())
    SKIP("PATTERNIST_SHOTS unset");
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    const auto shot = [&](const QString& name, int lanes, int steps, bool advanced,
                          Patternist::View::Mode mode, QSize sz) {
      Patternist::ProcessModel proc{
          TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr};
      auto p = big_pattern(lanes, steps);
      for(int i = 0; i < lanes; i++)
        for(int s = 0; s < steps; s++)
          p.lanes[i].pattern[s] = (s + i) % 3 == 0 ? Note::Note : Note::Rest;
      proc.setPattern(0, p);
      Patternist::View v{proc, nullptr};
      v.setMode(mode);
      v.setAdvanced(advanced);
      v.setZoom(ossia::quarter_duration<double> / 4. / 12.); // 12 px per sixteenth
      v.setWidth(sz.width());
      v.setHeight(sz.height());
      QImage img{sz, QImage::Format_ARGB32_Premultiplied};
      img.fill(score::Skin::instance().Background1.main.brush.color());
      {
        QPainter painter{&img};
        static_cast<QGraphicsItem&>(v).paint(&painter, nullptr, nullptr);
      }
      img.save(dir + "/" + name + ".png");
    };
    using M = Patternist::View::Mode;
    shot("editor_16x16_720x320", 16, 16, true, M::Fit, {720, 320});
    shot("editor_64x16_720x320", 64, 16, true, M::Fit, {720, 320});
    shot("editor_128x128_1600x900", 128, 128, true, M::Fit, {1600, 900});
    shot("editor_4x16_300x150", 4, 16, true, M::Fit, {300, 150});
    shot("layer_2x16_timeline_800x60", 2, 16, false, M::Timeline, {800, 60});
    shot("layer_32x16_timeline_800x120", 32, 16, false, M::Timeline, {800, 120});
    shot("node_8x16_fit_200x80", 8, 16, false, M::Fit, {200, 80});
    shot("node_8x128_fit_200x80", 8, 128, false, M::Fit, {200, 80});
  });
}

TEST_CASE("patternist: the editor window at its default size", "[.][midi][pattern][shots]")
{
  const QString dir = qEnvironmentVariable("PATTERNIST_SHOTS");
  if(dir.isEmpty())
    SKIP("PATTERNIST_SHOTS unset");
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    for(int lanes : {2, 8, 16, 32})
    {
      auto doc = score::test::new_document(ctx);
      REQUIRE(doc);
      auto proc = add_pattern_process(*doc);
      REQUIRE(proc);
      auto p = big_pattern(lanes, 16);
      for(int i = 0; i < lanes; i++)
        for(int s = 0; s < 16; s++)
          p.lanes[i].pattern[s] = (s + i) % 3 == 0 ? Note::Note : Note::Rest;
      proc->setPattern(0, p);
      auto editor = new Patternist::Editor{*proc, doc->context(), nullptr};
      editor->show();
      for(int i = 0; i < 20; i++)
        QApplication::processEvents();
      editor->grab().save(dir + QStringLiteral("/editor_window_%1_lanes.png").arg(lanes));
      delete editor;
    }
  });
}
