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
