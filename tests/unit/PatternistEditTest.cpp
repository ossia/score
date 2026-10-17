// Patternist's pattern operations and the timeline layout of its steps.

#include <ossia/detail/flicks.hpp>

#include <Patternist/PatternEdit.hpp>
#include <Patternist/PatternGrid.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Patternist;

namespace
{
Lane lane(uint8_t note, std::initializer_list<Note> steps)
{
  return Lane{std::vector<Note>{steps}, note};
}

constexpr Note N = Note::Note;
constexpr Note R = Note::Rest;
constexpr Note L = Note::Legato;

Pattern pattern(std::vector<Lane> lanes, int length, int division = 16)
{
  Pattern p;
  p.lanes = std::move(lanes);
  p.length = length;
  p.division = division;
  return p;
}

//! One pixel per sixteenth note at 120 BPM.
constexpr double sixteenth = ossia::quarter_duration<double> / 4.;
}

TEST_CASE(
    "patternist: normalize gives every lane the pattern's length", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N, R}), lane(38, {N, R, N, R, N, R})}, 4);
  normalize(p);
  CHECK(p.lanes[0].pattern.size() == 4);
  CHECK(p.lanes[0].pattern[2] == R);
  // Steps past the length are kept for when the pattern grows again.
  CHECK(p.lanes[1].pattern.size() == 6);

  p.length = 0;
  normalize(p);
  CHECK(p.length == 1);
  p.length = 1000;
  normalize(p);
  CHECK(p.length == Pattern::maxLength);
  CHECK(p.lanes[0].pattern.size() == std::size_t(Pattern::maxLength));
}

TEST_CASE(
    "patternist: setLength lengthens every lane, not only the first", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N, R, N, R}), lane(38, {N})}, 4);
  setLength(p, 8);
  CHECK(p.length == 8);
  for(const auto& l : p.lanes)
    CHECK(l.pattern.size() == 8);
  CHECK(p.lanes[0].pattern[2] == N);
}

TEST_CASE("patternist: rotate wraps the first length steps", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N, R, R, L, N})}, 4);
  rotate(p, 1);
  CHECK(p.lanes[0].pattern == std::vector<Note>{L, N, R, R, N});
  rotate(p, -1);
  CHECK(p.lanes[0].pattern == std::vector<Note>{N, R, R, L, N});
  rotate(p, 9);
  CHECK(p.lanes[0].pattern == std::vector<Note>{L, N, R, R, N});
}

TEST_CASE("patternist: clear, setStep and toggledNote", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N, L, R})}, 3);
  CHECK(toggledNote(R) == N);
  CHECK(toggledNote(N) == R);
  CHECK(toggledNote(L) == R);

  CHECK(setStep(p, 0, 2, N));
  CHECK_FALSE(setStep(p, 0, 2, N));
  CHECK_FALSE(setStep(p, 1, 0, N));
  CHECK_FALSE(setStep(p, 0, 7, N));
  CHECK(p.lanes[0].pattern[2] == N);

  clear(p);
  CHECK(p.lanes[0].pattern == std::vector<Note>{R, R, R});
}

TEST_CASE(
    "patternist: randomize keeps the lanes and only writes notes and rests",
    "[midi][pattern]")
{
  auto p = pattern({lane(36, {}), lane(255, {})}, 16);
  std::mt19937 rng{1234};
  randomize(p, rng);
  REQUIRE(p.lanes.size() == 2);
  CHECK(p.lanes[1].note == 255);
  int notes = 0;
  for(const auto& l : p.lanes)
  {
    REQUIRE(l.pattern.size() == 16);
    for(Note n : l.pattern)
    {
      CHECK(n != L);
      notes += n == N;
    }
  }
  CHECK(notes > 0);
  CHECK(notes < 32);
}

TEST_CASE("patternist: adding a lane copies the last one", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N, R}), lane(38, {R, N})}, 2);
  p.lanes[1].mute = true;
  addLane(p);
  REQUIRE(p.lanes.size() == 3);
  CHECK(p.lanes[2] == p.lanes[1]);

  removeLane(p, 0);
  CHECK(p.lanes.size() == 2);
  removeLane(p, 0);
  removeLane(p, 0);
  CHECK(p.lanes.size() == 1);
}

TEST_CASE("patternist: mute and solo decide what a lane plays", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N}), lane(38, {N}), lane(40, {L})}, 1);
  CHECK_FALSE(anySolo(p));
  CHECK(audibleNote(p, p.lanes[0], 0, false) == N);

  p.lanes[0].mute = true;
  CHECK(audibleNote(p, p.lanes[0], 0, anySolo(p)) == R);

  p.lanes[2].solo = true;
  CHECK(anySolo(p));
  CHECK(audibleNote(p, p.lanes[1], 0, true) == R);
  CHECK(audibleNote(p, p.lanes[2], 0, true) == L);
  CHECK(audibleNote(p, p.lanes[2], 5, true) == R);
}

TEST_CASE("patternist: lane labels", "[midi][pattern]")
{
  CHECK(laneLabel(36, LaneLabels::NameAndNumber) == QStringLiteral("C2 36"));
  CHECK(laneLabel(36, LaneLabels::Name) == QStringLiteral("C2"));
  CHECK(laneLabel(36, LaneLabels::Number) == QStringLiteral("36"));
  CHECK(laneLabel(36, LaneLabels::DrumName) == QStringLiteral("Kick"));
  CHECK(laneLabel(38, LaneLabels::DrumName) == QStringLiteral("Snare"));
  // Outside the General MIDI drum map, the note name.
  CHECK(laneLabel(90, LaneLabels::DrumName) == QStringLiteral("F#6"));
  CHECK(laneLabel(255, LaneLabels::Number) == QStringLiteral("Accent"));
  CHECK(laneLabel(254, LaneLabels::Name) == QStringLiteral("Slide"));
}

TEST_CASE(
    "patternist: timeline steps start on the first grid point of the process",
    "[midi][pattern]")
{
  const auto p = pattern({lane(36, {N, R, R, R})}, 4);

  SECTION("a process starting on a bar line")
  {
    TimeGrid g;
    const auto s = timelineSlots(p, g, sixteenth, 10.);
    REQUIRE(s.size() == 10);
    for(int i = 0; i < 10; i++)
    {
      CHECK(s[i].x0 == Catch::Approx(i));
      CHECK(s[i].step == i % 4);
      CHECK(s[i].repeat == (i >= 4));
    }
    CHECK(s[0].barStart);
    CHECK(s[4].beatStart);
    CHECK_FALSE(s[5].beatStart);
  }

  SECTION("a process starting between two grid points")
  {
    // 2.5 sixteenths in: the first step is at the third sixteenth.
    TimeGrid g;
    g.origin = TimeVal{int64_t(2.5 * sixteenth)};
    g.playStart = g.origin;
    const auto s = timelineSlots(p, g, sixteenth, 6.);
    REQUIRE(s.size() == 6);
    CHECK(s[0].x0 == Catch::Approx(0.5));
    CHECK(s[0].step == 0);
    CHECK_FALSE(s[0].repeat);
    CHECK(s[1].step == 1);
  }

  SECTION("a start offset shows the steps before the process starts as repeats")
  {
    TimeGrid g;
    g.playStart = TimeVal{int64_t(2 * sixteenth)};
    g.origin = TimeVal{0};
    const auto s = timelineSlots(p, g, sixteenth, 4.);
    REQUIRE(s.size() == 4);
    CHECK(s[0].step == 2);
    CHECK(s[0].repeat);
    CHECK(s[2].step == 0);
    CHECK_FALSE(s[2].repeat);
  }
}

TEST_CASE("patternist: the timeline grid restarts at every bar", "[midi][pattern]")
{
  // A 3/8 bar is six sixteenths.
  const auto p = pattern({lane(36, {N, R, R, R, R, R, R, R})}, 8, 16);

  TimeGrid g;
  g.signatures.push_back({TimeVal{0}, ossia::time_signature{3, 8}});
  const auto s = timelineSlots(p, g, sixteenth, 12.);
  REQUIRE(s.size() == 12);
  CHECK(s[0].barStart);
  CHECK(s[6].barStart);
  CHECK_FALSE(s[3].barStart);
  // A beat is an eighth in 3/8.
  CHECK(s[2].beatStart);
  CHECK_FALSE(s[1].beatStart);

  SECTION("a bar that is not a whole number of steps")
  {
    // Quarter-note steps in 3/8: the second step of a bar is cut short by
    // the next bar line, where the grid starts over.
    const auto q = pattern({lane(36, {N, R})}, 2, 4);
    const auto sq = timelineSlots(q, g, sixteenth, 18.);
    REQUIRE(sq.size() == 6);
    const double expected[] = {0., 4., 6., 10., 12., 16.};
    for(int i = 0; i < 6; i++)
      CHECK(sq[i].x0 == Catch::Approx(expected[i]));
    CHECK(sq[1].x1 == Catch::Approx(6.));
  }
}

TEST_CASE(
    "patternist: the timeline refuses to lay out absurd step counts", "[midi][pattern]")
{
  const auto p = pattern({lane(36, {N})}, 1, 64);
  TimeGrid g;
  CHECK(timelineSlots(p, g, sixteenth / 1000., 1e6, 1000).empty());
  CHECK(timelineSlots(p, g, 0., 100.).empty());
}

TEST_CASE(
    "patternist: a node lines its lanes up over the longest loop", "[midi][pattern]")
{
  // 16 sixteenths against 6 steps of a third of a whole note: a bar against two.
  auto p = pattern({lane(36, {N}), lane(42, {N})}, 16, 16);
  p.lanes[1].length = 6;
  p.lanes[1].division = 3;
  normalize(p);

  const auto layout = fitLayout(p, {4, 4}, 0., 320., 6.);
  CHECK(layout.span == Catch::Approx(8.));
  REQUIRE(layout.lanes.size() == 2);

  // The short lane plays twice: the second pass is a repeat of the same steps.
  const auto& fast = layout.lanes[0];
  REQUIRE(fast.size() == 32);
  CHECK(fast[16].step == 0);
  CHECK(fast[16].repeat);
  CHECK(fast[16].barStart);
  CHECK(fast[16].x0 == Catch::Approx(160.));
  CHECK_FALSE(fast[15].repeat);

  const auto& slow = layout.lanes[1];
  REQUIRE(slow.size() == 6);
  CHECK(slow[5].x1 == Catch::Approx(320.));
  CHECK_FALSE(slow[5].repeat);

  SECTION("what plays falls where the time is")
  {
    // The 20th sixteenth is the 5th step of the second pass.
    CHECK(playingSlot(fast, 20, 16, 16, layout.span) == 20);
    // Past the span, back to the first pass.
    CHECK(playingSlot(fast, 33, 16, 16, layout.span) == 1);
    CHECK(playingSlot(slow, 7, 6, 3, layout.span) == 1);
    CHECK(playingSlot(fast, -1, 16, 16, layout.span) == -1);
  }

  SECTION("steps too thin to aim at give each lane the whole width")
  {
    const auto narrow = fitLayout(p, {4, 4}, 0., 40., 6.);
    CHECK(narrow.span == 0.);
    CHECK(narrow.lanes[0].size() == 16);
    CHECK(narrow.lanes[1].size() == 6);
    CHECK(narrow.lanes[0].back().x1 == Catch::Approx(40.));
    CHECK(playingSlot(narrow.lanes[0], 20, 16, 16, narrow.span) == 4);
  }
}

TEST_CASE("patternist: cells alternate by beat, or by bar", "[midi][pattern]")
{
  // Sixteenths in 4/4: four cells per tone.
  const auto s = fitLaneSlots(16, 16, {4, 4}, 4., 0., 160.);
  REQUIRE(s.size() == 16);
  CHECK(s[3].shade == 0);
  CHECK(s[4].shade == 1);
  CHECK(s[8].shade == 0);

  // Eighths in 7/8: a step is a beat, so the bars alternate.
  const auto e = fitLaneSlots(7, 8, {7, 8}, 7., 0., 140.);
  REQUIRE(e.size() == 14);
  CHECK(e[1].shade == 0);
  CHECK(e[6].shade == 0);
  CHECK(e[7].shade == 1);

  // Same on the timeline, quarters in 3/4.
  TimeGrid g;
  g.signatures = {{TimeVal::zero(), {3, 4}}};
  const auto q = timelineSlots(4, 4, g, sixteenth, 48.);
  REQUIRE(q.size() == 12);
  CHECK(q[2].shade == 0);
  CHECK(q[3].shade == 1);
  CHECK(q[6].shade == 0);
  CHECK(q[3].index == 3);
  CHECK(slotWithIndex(q, 5) == 5);
  CHECK(slotWithIndex(q, 40) == -1);
}

TEST_CASE("patternist: Euclidean rhythms are written into the steps", "[midi][pattern]")
{
  int hits = 0;
  for(int i = 0; i < 8; i++)
    hits += euclidHit(i, 8, 3, 0);
  CHECK(hits == 3);
  CHECK(euclidHit(1, 8, 3, 1));
  CHECK_FALSE(euclidHit(0, 8, 0, 0));

  // Hits on 0 and 4 over the 8 steps the lane plays.
  auto p = pattern({lane(36, {R, N, R, R, R, R, R, R})}, 8);
  normalize(p);
  auto& l = p.lanes[0];

  SECTION("replace")
  {
    euclidean(p, l, 2, 0, Combine::Replace);
    CHECK(l.pattern[0] == N);
    CHECK(l.pattern[1] == R);
    CHECK(l.pattern[4] == N);
  }
  SECTION("add")
  {
    euclidean(p, l, 2, 0, Combine::Add);
    CHECK(l.pattern[0] == N);
    CHECK(l.pattern[1] == N);
  }
  SECTION("mask")
  {
    euclidean(p, l, 2, 1, Combine::Mask);
    CHECK(l.pattern[1] == N);
    CHECK(l.pattern[0] == R);
  }
  SECTION("over the lane's own length")
  {
    l.length = 4;
    normalize(p);
    euclidean(p, l, 2, 0);
    CHECK(l.pattern[2] == N);
    // Past the lane's loop, nothing is written.
    CHECK(l.pattern[5] == R);
  }
}

TEST_CASE("patternist: steps copied across lanes", "[midi][pattern]")
{
  // Lanes 46 42 38 36: a beat of 38 and 36 pasted on 46 and 42.
  auto p = pattern(
      {lane(46, {R, R, R, R}), lane(42, {R, R, R, R}), lane(38, {R, N, R, L}),
       lane(36, {N, R, N, R})},
      4);
  normalize(p);
  p.lanes[2].params[1].velocity = 0.5f;

  const auto block = copySteps(p, {2, {{1, 2, 3}, {1, 2, 3}}});
  REQUIRE(block.lanes.size() == 2);
  CHECK(block.length == 3);

  pasteSteps(p, block, 0, {0, 0});
  CHECK(p.lanes[0].pattern == std::vector<Note>{N, R, L, R});
  CHECK(p.lanes[1].pattern == std::vector<Note>{R, N, R, R});
  CHECK(p.lanes[0].params[0].velocity == 0.5f);
  // The sources are untouched.
  CHECK(p.lanes[3].pattern == std::vector<Note>{N, R, N, R});

  SECTION("pasted steps loop over the lane, and past the last lane are dropped")
  {
    pasteSteps(p, block, 3, {2});
    CHECK(p.lanes[3].pattern == std::vector<Note>{L, R, N, R});
    CHECK(p.lanes[3].pattern.size() == 4);
  }
  SECTION("lanes of different rates keep their own steps")
  {
    // A beat is one step of a quarter-note lane and four of the others.
    p.lanes[0].division = 4;
    p.lanes[0].length = 4;
    normalize(p);
    const auto mixed = copySteps(p, {0, {{1}, {4 % 4, 5 % 4, 6 % 4, 7 % 4}}});
    CHECK(mixed.lanes[0].pattern.size() == 1);
    CHECK(mixed.lanes[1].pattern.size() == 4);
    CHECK(mixed.length == 4);
  }
  SECTION("clear")
  {
    clearSteps(p, {0, {{0, 1}, {0, 1}}});
    CHECK(p.lanes[0].pattern == std::vector<Note>{R, R, L, R});
    CHECK(p.lanes[0].params[0].isDefault());
  }
}

TEST_CASE("patternist: step parameters at random", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N, N, N, N}), lane(38, {N, N, N, N})}, 4);
  normalize(p);
  std::mt19937 rng{1};
  randomizeField(p, rng, StepField::Velocity, 0);
  bool changed = false;
  for(const auto& s : p.lanes[0].params)
  {
    CHECK(s.velocity >= 0.4f);
    changed |= s.velocity != 1.f;
  }
  CHECK(changed);
  for(const auto& s : p.lanes[1].params)
    CHECK(s.isDefault());

  randomizeField(p, rng, StepField::Ratchet, -1);
  for(const auto& l : p.lanes)
    for(const auto& s : l.params)
      CHECK((s.ratchet >= 1 && s.ratchet <= 4));

  resetField(p, StepField::Velocity, -1);
  resetField(p, StepField::Ratchet, -1);
  for(const auto& l : p.lanes)
    for(const auto& s : l.params)
      CHECK(s.isDefault());
}

TEST_CASE("patternist: which tied steps strike a note", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N, L, L, R, L, L})}, 6);
  normalize(p);
  auto& l = p.lanes[0];

  // A tie after a note only holds it: its velocity is the note's.
  CHECK_FALSE(tieStrikes(p, l, 1));
  CHECK(struckStep(p, l, 2) == 0);
  // After a rest a tie strikes.
  CHECK(tieStrikes(p, l, 4));
  CHECK(struckStep(p, l, 5) == 4);
  CHECK(struckStep(p, l, 0) == 0);

  SECTION("a ratchet or another pitch strikes again")
  {
    l.params[2].ratchet = 2;
    CHECK(tieStrikes(p, l, 2));
    CHECK(struckStep(p, l, 2) == 2);
    l.params[2].ratchet = 1;
    l.params[2].pitch = 3;
    CHECK(tieStrikes(p, l, 2));
  }
}

TEST_CASE("patternist: an Alt-drag can reach a parameter's whole range", "[midi][pattern]")
{
  std::mt19937 rng{7};
  for(auto f : {StepField::PitchRandom, StepField::Pitch, StepField::VelocityRandom,
                StepField::Ratchet})
  {
    const auto r = fieldRange(f);
    float lo = r.max, hi = r.min;
    for(int i = 0; i < 2000; i++)
    {
      const float v = anyFieldValue(f, rng);
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
    CHECK(lo >= r.min);
    CHECK(hi <= r.max);
    // Close to both ends, not the mild range the Random button uses.
    CHECK(hi - lo >= 0.9f * (r.max - r.min));
  }
}

TEST_CASE("patternist: transposing moves every note lane", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N}), lane(120, {N}), lane(255, {N}), lane(254, {N})}, 1);
  CHECK(transpose(p, 12));
  CHECK(p.lanes[0].note == 48);
  CHECK(p.lanes[1].note == 127);
  // The accent and slide lanes stay.
  CHECK(p.lanes[2].note == 255);
  CHECK(p.lanes[3].note == 254);
  CHECK(transpose(p, -1));
  CHECK(p.lanes[0].note == 47);
  p.lanes[0].note = 0;
  p.lanes[1].note = 0;
  CHECK_FALSE(transpose(p, -12));
}
