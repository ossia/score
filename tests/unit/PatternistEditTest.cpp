// Patternist's pattern operations and the timeline layout of its steps.

#include <ossia/detail/flicks.hpp>

#include <Patternist/PatternEdit.hpp>
#include <Patternist/PatternGrid.hpp>
#include <Patternist/PatternMidiImport.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libremidi/writer.hpp>

#include <QByteArray>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

#include <cmath>
#include <limits>
#include <random>
#include <sstream>

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

TEST_CASE("patternist: a lane is added empty, under the one given", "[midi][pattern]")
{
  auto p = pattern({lane(38, {N, R}), lane(36, {R, N})}, 2);
  p.lanes[1].mute = true;
  p.lanes[1].length = 1;
  normalize(p);

  SECTION("under the last note lane when no lane is given")
  {
    CHECK(addLane(p) == 2);
    REQUIRE(p.lanes.size() == 3);
    const Lane& l = p.lanes[2];
    CHECK(l.pattern == std::vector<Note>(2, R));
    CHECK(l.params == std::vector<StepParams>(2));
    // Nothing of its neighbour but a note next to its own.
    CHECK(l.note == 35);
    CHECK(!l.mute);
    CHECK(l.length == 0);
    CHECK(l.velocity == Lane{}.velocity);
    CHECK(p.lanes[0].pattern == std::vector<Note>({N, R}));
    CHECK(p.lanes[1].pattern == std::vector<Note>({R, N}));
  }
  SECTION("under the lane given, with a note no lane plays")
  {
    CHECK(addLane(p, 0) == 1);
    REQUIRE(p.lanes.size() == 3);
    CHECK(p.lanes[0].note == 38);
    CHECK(p.lanes[1].note == 37);
    CHECK(p.lanes[1].pattern == std::vector<Note>(2, R));
    CHECK(p.lanes[2].note == 36);

    // 37 and 36 are taken: the next one down that is free.
    CHECK(addLane(p, 0) == 1);
    CHECK(p.lanes[1].note == 35);
    CHECK(addLane(p, 3) == 4);
    CHECK(p.lanes[4].note == 34);
  }
  SECTION("at the bottom of the range, the note goes up")
  {
    auto q = pattern({lane(1, {N}), lane(0, {N})}, 1);
    CHECK(addLane(q, 1) == 2);
    CHECK(q.lanes[2].note == 2);
  }
  SECTION("a full pattern takes no more")
  {
    while(std::ssize(p.lanes) < Pattern::maxLanes)
      REQUIRE(addLane(p) >= 0);
    CHECK(addLane(p) == -1);
    CHECK(addLane(p, 0) == -1);
    CHECK(std::ssize(p.lanes) == Pattern::maxLanes);
  }
}

TEST_CASE("patternist: removing lanes leaves one", "[midi][pattern]")
{
  auto p = pattern({lane(36, {N, R}), lane(38, {R, N})}, 2);
  removeLane(p, 0);
  CHECK(p.lanes.size() == 1);
  removeLane(p, 0);
  CHECK(p.lanes.size() == 1);
}

TEST_CASE("patternist: a lane moves up and down with all it holds", "[midi][pattern]")
{
  auto p = pattern({lane(40, {N, R}), lane(38, {R, N}), lane(36, {N, N})}, 2);
  normalize(p);
  p.lanes[0].mute = true;
  p.lanes[0].params[0].velocity = 0.25f;
  const Lane a = p.lanes[0], b = p.lanes[1], c = p.lanes[2];

  CHECK(moveLane(p, 0, 1) == 1);
  CHECK(p.lanes == std::vector<Lane>{b, a, c});
  CHECK(moveLane(p, 1, 1) == 2);
  CHECK(p.lanes == std::vector<Lane>{b, c, a});
  // The ends of the list stop it.
  CHECK(moveLane(p, 2, 1) == -1);
  CHECK(moveLane(p, 0, -1) == -1);
  CHECK(p.lanes == std::vector<Lane>{b, c, a});
  CHECK(moveLane(p, 2, -1) == 1);
  CHECK(moveLane(p, 1, -1) == 0);
  CHECK(p.lanes == std::vector<Lane>{a, b, c});
  // Further than the list goes: to its end.
  CHECK(moveLane(p, 0, 10) == 2);
  CHECK(p.lanes == std::vector<Lane>{b, c, a});
  CHECK(moveLane(p, 5, 1) == -1);
  CHECK(moveLane(p, -1, 1) == -1);
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
    "patternist: timeline steps count from the bar line the process starts in",
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
    // 2.5 sixteenths in: the first point is the bar's fourth sixteenth, step 3.
    TimeGrid g;
    g.origin = TimeVal{int64_t(2.5 * sixteenth)};
    g.playStart = g.origin;
    const auto s = timelineSlots(p, g, sixteenth, 6.);
    REQUIRE(s.size() == 6);
    CHECK(s[0].x0 == Catch::Approx(0.5));
    CHECK(s[0].step == 3);
    CHECK(s[0].index == 3);
    CHECK_FALSE(s[0].repeat);
    CHECK(s[1].step == 0);
    CHECK(s[1].repeat);
  }

  SECTION("a process starting mid-bar in a later bar")
  {
    // Bar 2, beat 2 and a half: six sixteenths into the bar.
    const auto q = pattern({lane(36, {N})}, 16);
    TimeGrid g;
    g.origin = TimeVal{int64_t(22 * sixteenth)};
    g.playStart = g.origin;
    const auto s = timelineSlots(q, g, sixteenth, 12.);
    REQUIRE(s.size() == 12);
    CHECK(s[0].step == 6);
    CHECK(s[0].index == 6);
    CHECK(s[10].step == 0);
    CHECK(s[10].barStart);
    CHECK(s[10].repeat);
  }

  SECTION("a start offset counts from the bar line the content starts in")
  {
    // The content starts two sixteenths into the bar, the process plays from
    // the sixth.
    const auto q = pattern({lane(36, {N})}, 16);
    TimeGrid g;
    g.origin = TimeVal{int64_t(2 * sixteenth)};
    g.playStart = TimeVal{int64_t(6 * sixteenth)};
    const auto s = timelineSlots(q, g, sixteenth, 8.);
    REQUIRE(s.size() == 8);
    CHECK(s[0].step == 2);
    CHECK(s[0].repeat);
    CHECK(s[4].step == 6);
    CHECK(s[4].index == 6);
    CHECK_FALSE(s[4].repeat);
  }

  SECTION("a start offset shows the steps before the process starts as repeats")
  {
    TimeGrid g;
    g.playStart = TimeVal{int64_t(2 * sixteenth)};
    g.origin = TimeVal{0};
    const auto s = timelineSlots(p, g, sixteenth, 4.);
    REQUIRE(s.size() == 4);
    CHECK(s[0].step == 0);
    CHECK(s[0].repeat);
    CHECK(s[2].step == 2);
    CHECK(s[2].index == 2);
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

TEST_CASE("patternist: accent and slide lanes stay at the bottom", "[midi][pattern]")
{
  Pattern p;
  p.length = 4;
  p.lanes.push_back(Lane{std::vector<Note>(4, Note::Note), 36});
  normalize(p);

  CHECK(addControlLane(p, accentLane));
  CHECK(!addControlLane(p, accentLane));
  CHECK(addControlLane(p, slideLane));
  REQUIRE(p.lanes.size() == 3);
  CHECK(p.lanes[1].note == accentLane);
  CHECK(p.lanes[1].pattern == std::vector<Note>(4, Note::Rest));

  // A new lane goes above the accent and the slide, unless told otherwise.
  CHECK(addLane(p) == 1);
  REQUIRE(p.lanes.size() == 4);
  CHECK(p.lanes[1].note == 35);
  CHECK(p.lanes[1].pattern == std::vector<Note>(4, Note::Rest));
  CHECK(p.lanes[2].note == accentLane);
  CHECK(p.lanes[3].note == slideLane);
  CHECK(hasLane(p, slideLane));

  // Under the accent: a note lane, named after the nearest one above.
  CHECK(addLane(p, 2) == 3);
  CHECK(p.lanes[3].note == 34);
  CHECK(p.lanes[4].note == slideLane);
}

TEST_CASE("patternist: the gate random is a parameter", "[midi][pattern]")
{
  StepParams s;
  setStepField(s, StepField::GateRandom, 2.f);
  CHECK(stepField(s, StepField::GateRandom) == 1.f);
  CHECK(s.gate == StepParams{}.gate);
}

TEST_CASE("patternist: whole lanes copied over others", "[midi][pattern]")
{
  auto p = pattern(
      {lane(46, {R, R, R, R}), lane(42, {R, R, R, R}), lane(38, {R, N, R, N}),
       lane(36, {N, R, N, R})},
      4);
  normalize(p);
  p.lanes[3].params[0].velocity = 0.5f;
  p.lanes[3].velocity = 0.25f;
  p.lanes[0].mute = true;

  const Pattern copied = copyLanes(p, {2, 3});
  REQUIRE(copied.lanes.size() == 2);
  CHECK(pasteLanes(p, copied, {0, 1}));
  // What they play moves; which note, mute, and how loud, stay.
  CHECK(p.lanes[0].pattern == p.lanes[2].pattern);
  CHECK(p.lanes[1].pattern == p.lanes[3].pattern);
  CHECK(p.lanes[0].note == 46);
  CHECK(p.lanes[0].mute);
  CHECK(p.lanes[1].params[0].velocity == 0.5f);
  CHECK(p.lanes[1].velocity == Lane{}.velocity);
  CHECK(!pasteLanes(p, copied, {0, 1}));

  // More lanes copied than targets: as far as the targets go.
  CHECK(pasteLanes(p, copyLanes(p, {3, 2}), {2}));
  CHECK(p.lanes[2].pattern == p.lanes[3].pattern);

  clearLanes(p, {1, 2});
  CHECK(p.lanes[1].pattern == std::vector<Note>(4, R));
  CHECK(p.lanes[1].params[0] == StepParams{});
  CHECK(p.lanes[3].pattern == std::vector<Note>({N, R, N, R}));
}

TEST_CASE("patternist: a lane copied from a pattern of another length", "[midi][pattern]")
{
  auto from = pattern({lane(36, {N, R, R})}, 3);
  auto to = pattern({lane(38, {R, R, R, R, R, R, R, R})}, 8);
  CHECK(pasteLanes(to, copyLanes(from, {0}), {0}));
  CHECK(to.lanes[0].length == 3);
  CHECK(writtenNote(to, to.lanes[0], 0) == N);
}

// ---------------------------------------------------------------------------
// MIDI files written over a pattern.
namespace
{
struct file_note
{
  int tick;
  int length;
  int channel;
  int pitch;
  int velocity;
  int track{0};
};

//! A Standard MIDI file at 480 ticks per quarter, of `end` ticks. With one
//! track it is written as type 0, as type 1 otherwise.
QByteArray midi_file(const std::vector<file_note>& notes, int end, int tracks = 1)
{
  libremidi::writer w;
  w.ticksPerQuarterNote = 480;
  for(int t = 0; t < tracks; t++)
  {
    struct ev
    {
      int tick;
      libremidi::message m;
    };
    std::vector<ev> events;
    for(const auto& n : notes)
    {
      if(n.track != t)
        continue;
      events.push_back(
          {n.tick, libremidi::channel_events::note_on(n.channel, n.pitch, n.velocity)});
      events.push_back(
          {n.tick + n.length, libremidi::channel_events::note_off(n.channel, n.pitch, 0)});
    }
    std::stable_sort(events.begin(), events.end(), [](const ev& a, const ev& b) {
      return a.tick < b.tick;
    });
    int prev = 0;
    if(t == 0)
      w.add_event(0, 0, libremidi::meta_events::time_signature(4, 4, 24, 8));
    for(const auto& e : events)
    {
      w.add_event(e.tick - prev, t, e.m);
      prev = e.tick;
    }
    // libremidi's writer ends a track right after its last event: a
    // controller there holds the length of the file.
    w.add_event(
        std::max(0, end - prev), t, libremidi::channel_events::control_change(1, 7, 100));
  }
  std::ostringstream out;
  w.write(out);
  const auto s = std::move(out).str();
  return QByteArray(s.data(), qsizetype(s.size()));
}

//! The steps of a lane that strike a note.
std::vector<int> hits(const Pattern& p, int note)
{
  std::vector<int> res;
  for(const Lane& l : p.lanes)
    if(l.note == note)
      for(int i = 0; i < p.length; i++)
        if(l.pattern[i] == N)
          res.push_back(i);
  return res;
}

const Lane& lane_of(const Pattern& p, int note)
{
  for(const Lane& l : p.lanes)
    if(l.note == note)
      return l;
  FAIL("no lane plays " << note);
  return p.lanes.front();
}

//! What the engine sends for a step: see pattern_node::play_step.
int played_velocity(const Lane& l, int step)
{
  return int(std::lround(l.params[step].velocity * l.velocity * 127.f));
}

constexpr int sixteenth_ticks = 120;

//! A bar of 4/4 on the drum channel: kick on the beats, snare on 2 and 4,
//! closed hat on the eighths, softer off the beat.
std::vector<file_note> drum_loop()
{
  std::vector<file_note> n;
  for(int beat = 0; beat < 4; beat++)
    n.push_back({beat * 480, 60, 10, 36, 110});
  for(int beat : {1, 3})
    n.push_back({beat * 480, 60, 10, 38, 120});
  for(int e = 0; e < 8; e++)
    n.push_back({e * 240, 30, 10, 42, e % 2 ? 60 : 90});
  return n;
}
}

TEST_CASE("patternist: a 4/4 drum loop lands on a sixteenth grid", "[midi][pattern][import]")
{
  // The default pattern: snare over kick, 16 steps of a sixteenth.
  auto p = pattern(
      {lane(38, {R, R, N, R, N, R, R, R, N, R, N, R, N, R, N, R}),
       lane(36, {N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N})},
      16);
  normalize(p);
  p.lanes[1].length = 5;
  p.lanes[1].division = 8;
  p.lanes[1].mute = true;

  const MidiClip clip = readMidiFile(midi_file(drum_loop(), 4 * 480));
  REQUIRE(clip.notes.size() == 14);
  CHECK(clip.length == 4.);
  CHECK(clip.notes.front().drum);

  const MidiMatch m = matchToGrid(p, clip);
  CHECK(m.notes == 13);
  CHECK(m.merged == 0);
  CHECK(m.dropped == 1);
  const Pattern& r = m.pattern;
  CHECK(r.length == 16);
  CHECK(r.division == 16);

  // The hat is new and the highest: above the two lanes there were, which
  // keep their place.
  REQUIRE(r.lanes.size() == 3);
  CHECK(r.lanes[0].note == 42);
  CHECK(r.lanes[1].note == 38);
  CHECK(r.lanes[2].note == 36);

  // What the lanes played is gone, and each lane keeps its grid: the kick's
  // is 5 steps of an eighth, where its fourth beat falls past the end.
  CHECK(hits(r, 36) == std::vector<int>{0, 2, 4});
  CHECK(hits(r, 38) == std::vector<int>{4, 12});
  CHECK(hits(r, 42) == std::vector<int>{0, 2, 4, 6, 8, 10, 12, 14});
  CHECK(r.lanes[2].length == 5);
  CHECK(r.lanes[2].division == 8);
  CHECK(r.lanes[2].mute);
  // The new lane follows the pattern.
  CHECK(r.lanes[0].length == 0);
  CHECK(r.lanes[0].division == 0);

  // Velocities play as the file has them.
  const Lane& hat = lane_of(r, 42);
  CHECK(played_velocity(hat, 0) == 90);
  CHECK(played_velocity(hat, 2) == 60);
  CHECK(played_velocity(lane_of(r, 36), 2) == 110);
  CHECK(played_velocity(lane_of(r, 38), 4) == 120);

  // On the grid and on the drum channel: nothing but the velocity is set.
  for(const Lane& l : r.lanes)
    for(int i = 0; i < r.length; i++)
    {
      StepParams sp = l.params[i];
      sp.velocity = 1.f;
      CHECK(sp == StepParams{});
      CHECK(l.pattern[i] != L);
    }
}

TEST_CASE("patternist: notes off the grid go to the nearest step", "[midi][pattern][import]")
{
  auto p = pattern({lane(60, {R})}, 16);
  normalize(p);

  SECTION("eighth triplets keep their place as offsets")
  {
    std::vector<file_note> n;
    for(int i = 0; i < 6; i++)
      n.push_back({i * 160, 100, 1, 60, 100});
    const MidiMatch m = matchToGrid(p, readMidiFile(midi_file(n, 960)));
    const Pattern& r = m.pattern;
    CHECK(m.notes == 6);
    CHECK(r.length == 16);
    // 0, 1.33, 2.67, 4, 5.33 and 6.67 sixteenths.
    CHECK(hits(r, 60) == std::vector<int>{0, 1, 3, 4, 5, 7});
    const Lane& l = lane_of(r, 60);
    CHECK(l.params[0].offset == 0.f);
    CHECK(l.params[1].offset == Catch::Approx(1. / 3.));
    CHECK(l.params[3].offset == Catch::Approx(-1. / 3.));
    CHECK(l.params[4].offset == 0.f);
    // Where each plays is where the file has it.
    for(int i = 0; i < 6; i++)
    {
      const int step = hits(r, 60)[i];
      CHECK((step + l.params[step].offset) * sixteenth_ticks == Catch::Approx(i * 160.));
    }
  }
  SECTION("a few ticks off is on the grid")
  {
    const MidiMatch m = matchToGrid(
        p, readMidiFile(midi_file(
               {{3, 100, 10, 60, 100}, {480 - 4, 100, 10, 60, 100}, {960 + 5, 100, 10, 60, 100}},
               1920)));
    CHECK(hits(m.pattern, 60) == std::vector<int>{0, 4, 8});
    for(int step : {0, 4, 8})
      CHECK(lane_of(m.pattern, 60).params[step].offset == 0.f);
  }
  SECTION("two notes of a pitch on one step: the nearest stays")
  {
    // At 0, 1 and 1.25 sixteenths.
    const MidiMatch m = matchToGrid(
        p, readMidiFile(midi_file(
               {{0, 20, 10, 60, 50}, {150, 20, 10, 60, 100}, {120, 20, 10, 60, 70}}, 1920)));
    CHECK(m.notes == 2);
    CHECK(m.merged == 1);
    CHECK(hits(m.pattern, 60) == std::vector<int>{0, 1});
    const Lane& l = lane_of(m.pattern, 60);
    CHECK(l.params[1].offset == 0.f);
    CHECK(played_velocity(l, 1) == 70);
  }
  SECTION("a note just before the end leads into the first step")
  {
    const MidiMatch m = matchToGrid(
        p, readMidiFile(midi_file({{1920 - 40, 30, 10, 60, 100}}, 1920)));
    CHECK(m.pattern.length == 16);
    CHECK(hits(m.pattern, 60) == std::vector<int>{0});
    CHECK(lane_of(m.pattern, 60).params[0].offset == Catch::Approx(-1. / 3.));
  }
  SECTION("another division is kept")
  {
    p.division = 8;
    p.length = 8;
    const MidiMatch m = matchToGrid(p, readMidiFile(midi_file(drum_loop(), 1920)));
    CHECK(m.pattern.division == 8);
    CHECK(m.pattern.length == 8);
    CHECK(hits(m.pattern, 42) == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7});
    CHECK(hits(m.pattern, 36) == std::vector<int>{0, 2, 4, 6});
  }
}

TEST_CASE("patternist: a file longer than the pattern leaves its length and rate", "[midi][pattern][import]")
{
  auto p = pattern({lane(36, {N})}, 16);
  normalize(p);

  // Two bars and a beat of quarters.
  std::vector<file_note> quarters;
  for(int beat = 0; beat < 9; beat++)
    quarters.push_back({beat * 480, 60, 10, 36, 100});

  SECTION("what starts past the end is left out")
  {
    const MidiMatch m = matchToGrid(p, readMidiFile(midi_file(quarters, 9 * 480)));
    CHECK(m.pattern.length == 16);
    CHECK(m.pattern.division == 16);
    CHECK(m.notes == 4);
    CHECK(m.dropped == 5);
    CHECK(hits(m.pattern, 36) == std::vector<int>{0, 4, 8, 12});
  }
  SECTION("a silent end changes nothing")
  {
    const MidiMatch m
        = matchToGrid(p, readMidiFile(midi_file({{0, 60, 10, 36, 100}}, 2 * 1920)));
    CHECK(m.pattern.length == 16);
    CHECK(m.dropped == 0);
  }
  SECTION("a shorter file leaves the length")
  {
    const MidiMatch m
        = matchToGrid(p, readMidiFile(midi_file({{480, 60, 10, 36, 100}}, 960)));
    CHECK(m.pattern.length == 16);
    CHECK(hits(m.pattern, 36) == std::vector<int>{4});
  }
  SECTION("a lane of its own length and rate is filled on its own grid")
  {
    // Six steps of a quarter.
    p.lanes[0].length = 6;
    p.lanes[0].division = 4;
    const MidiMatch m = matchToGrid(p, readMidiFile(midi_file(quarters, 9 * 480)));
    CHECK(m.pattern.length == 16);
    CHECK(m.pattern.division == 16);
    const Lane& l = lane_of(m.pattern, 36);
    CHECK(l.length == 6);
    CHECK(l.division == 4);
    CHECK(m.notes == 6);
    CHECK(m.dropped == 3);
    CHECK(hits(m.pattern, 36) == std::vector<int>{0, 1, 2, 3, 4, 5});
  }
  SECTION("another pattern grid is kept as it is")
  {
    p.length = 12;
    p.division = 8;
    const MidiMatch m = matchToGrid(p, readMidiFile(midi_file(quarters, 9 * 480)));
    CHECK(m.pattern.length == 12);
    CHECK(m.pattern.division == 8);
    // Quarters on an eighth grid: every other step, six of them in twelve.
    CHECK(hits(m.pattern, 36) == std::vector<int>{0, 2, 4, 6, 8, 10});
    CHECK(m.dropped == 3);
  }
}

TEST_CASE("patternist: type 0 and type 1 files give the same pattern", "[midi][pattern][import]")
{
  auto p = pattern({lane(38, {R}), lane(36, {R})}, 16);
  normalize(p);

  // Drums and a bass line: one track, or one each.
  auto notes = drum_loop();
  const std::vector<file_note> bass
      = {{0, 480, 2, 40, 100}, {480, 110, 2, 43, 80}, {960, 60, 2, 40, 100},
         {1440, 240, 2, 45, 90}};
  auto split = notes;
  for(auto n : bass)
  {
    notes.push_back(n);
    n.track = 1;
    split.push_back(n);
  }
  const QByteArray type0 = midi_file(notes, 1920, 1);
  const QByteArray type1 = midi_file(split, 1920, 2);
  // Byte 9 of the header is the format.
  REQUIRE(type0[9] == 0);
  REQUIRE(type1[9] == 1);

  const MidiMatch a = matchToGrid(p, readMidiFile(type0));
  const MidiMatch b = matchToGrid(p, readMidiFile(type1));
  CHECK(a.notes == 18);
  CHECK(a.pattern == b.pattern);

  const Pattern& r = a.pattern;
  std::vector<int> order;
  for(const Lane& l : r.lanes)
    order.push_back(l.note);
  CHECK(order == std::vector<int>{45, 43, 42, 40, 38, 36});

  // Off the drum channel the lengths count: a quarter is tied over its
  // steps, a short note gets a gate, one about a step long fills it.
  const Lane& e = lane_of(r, 40);
  CHECK(e.pattern[0] == N);
  CHECK(e.pattern[1] == L);
  CHECK(e.pattern[2] == L);
  CHECK(e.pattern[3] == L);
  CHECK(e.pattern[4] == R);
  CHECK(e.params[0].gate == 1.f);
  CHECK(e.pattern[8] == N);
  CHECK(e.params[8].gate == Catch::Approx(0.5));
  CHECK(lane_of(r, 43).params[4].gate == 1.f);
  CHECK(lane_of(r, 43).pattern[5] == R);
  const Lane& a2 = lane_of(r, 45);
  CHECK(a2.pattern[12] == N);
  CHECK(a2.pattern[13] == L);
  CHECK(a2.pattern[14] == R);
  // The drums' lengths say nothing.
  CHECK(lane_of(r, 42).params[0].gate == 1.f);
}

TEST_CASE("patternist: files without notes leave the pattern alone", "[midi][pattern][import]")
{
  auto p = pattern({lane(38, {N, R, N, R}), lane(36, {R, N, R, N})}, 4);
  normalize(p);

  const auto unchanged = [&](const QByteArray& data) {
    const MidiClip clip = readMidiFile(data);
    CHECK(clip.notes.empty());
    const MidiMatch m = matchToGrid(p, clip);
    CHECK(m.notes == 0);
    CHECK(m.pattern == p);
  };

  SECTION("no data")
  {
    unchanged({});
  }
  SECTION("a file with a track and no event")
  {
    unchanged(midi_file({}, 1920));
  }
  SECTION("something else")
  {
    unchanged("BD x---x---x---x---\n");
    unchanged(QByteArray(4096, '\xff'));
    unchanged(QByteArray(4096, '\0'));
  }
  SECTION("a file cut anywhere")
  {
    const QByteArray whole = midi_file(drum_loop(), 1920);
    for(int n = 0; n < whole.size(); n++)
    {
      // Whatever could be read: never past the pattern's limits.
      const MidiMatch m = matchToGrid(p, readMidiFile(whole.left(n)));
      CHECK(m.pattern.length <= Pattern::maxLength);
      CHECK(std::ssize(m.pattern.lanes) <= Pattern::maxLanes);
      for(const Lane& l : m.pattern.lanes)
        CHECK(std::ssize(l.pattern) >= m.pattern.length);
    }
  }
  SECTION("bytes at random after a header")
  {
    QByteArray data = midi_file(drum_loop(), 1920);
    std::mt19937 rng{7};
    for(int round = 0; round < 200; round++)
    {
      QByteArray d = data;
      for(int k = 0; k < 8; k++)
        d[14 + int(rng() % (d.size() - 14))] = char(rng());
      const MidiMatch m = matchToGrid(p, readMidiFile(d));
      CHECK(m.pattern.length <= Pattern::maxLength);
      CHECK(std::ssize(m.pattern.lanes) <= Pattern::maxLanes);
    }
  }
  SECTION("notes that are not numbers")
  {
    MidiClip clip;
    clip.length = std::numeric_limits<double>::infinity();
    clip.notes.push_back({std::numeric_limits<double>::quiet_NaN(), 1., 60, 100, false});
    clip.notes.push_back({-4., 1., 60, 100, false});
    clip.notes.push_back({1e300, 1., 60, 100, false});
    clip.notes.push_back({0., std::numeric_limits<double>::quiet_NaN(), 62, 100, false});
    const MidiMatch m = matchToGrid(p, clip);
    CHECK(m.pattern.length <= Pattern::maxLength);
    CHECK(hits(m.pattern, 62) == std::vector<int>{0});
  }
}

TEST_CASE("patternist: more pitches than lanes", "[midi][pattern][import]")
{
  // Every pitch there is, over a pattern that already has lanes of its own.
  Pattern p;
  p.length = 16;
  for(int i = 0; i < 100; i++)
    p.lanes.push_back(Lane{std::vector<Note>(16, N), 60});
  addControlLane(p, accentLane);
  normalize(p);

  MidiClip clip;
  clip.length = 4.;
  for(int pitch = 0; pitch < 128; pitch++)
    clip.notes.push_back({0., 0.25, uint8_t(pitch), 100, true});
  const MidiMatch m = matchToGrid(p, clip);
  CHECK(std::ssize(m.pattern.lanes) == Pattern::maxLanes);
  CHECK(m.notes + m.dropped == 128);
  CHECK(m.dropped == 128 - 28);
  // The accent closes the list still, with the steps it had.
  CHECK(m.pattern.lanes.back().note == accentLane);
  CHECK(m.pattern.lanes.back() == p.lanes.back());
}

TEST_CASE("patternist: a MIDI file among what is dropped", "[midi][pattern][import]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto write = [&](const QString& name, const QByteArray& data) {
    QFile f{dir.filePath(name)};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(data);
    return QUrl::fromLocalFile(f.fileName());
  };

  QMimeData mime;
  CHECK(!midiFromMime(mime));
  mime.setUrls(
      {write("notes.txt", "hello"), write("empty.mid", midi_file({}, 480)),
       write("loop.MID", midi_file(drum_loop(), 1920))});
  const auto clip = midiFromMime(mime);
  REQUIRE(clip);
  CHECK(clip->notes.size() == 14);

  QMimeData other;
  other.setUrls({write("loop.wav", midi_file(drum_loop(), 1920))});
  CHECK(!midiFromMime(other));
}
