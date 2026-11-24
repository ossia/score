// The automation curve that recorded values draw (Recording::curveOf), what
// "Record automations from here" places:
//  - it spans the box: the first value holds from the start, the last up to
//    the end;
//  - stepped, a value holds until the next one; else it goes from one to the
//    next;
//  - its range is the values', the segments from 0 to 1 in it;
//  - a vector's axes are separate curves; a value without that axis, or no
//    value at all, gives none.

#include <Recording/Takes/RecordedCurves.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Recording;
using Catch::Approx;

namespace
{
constexpr int64_t ms = 705600;

//! y of the curve at x, from its segments (power segments of gamma 1: lines)
double at(const RecordedCurve& c, double x)
{
  for(const auto& s : c.segments)
    if(x >= s.start.x() && x <= s.end.x())
    {
      if(s.end.x() == s.start.x())
        return s.end.y();
      const double t = (x - s.start.x()) / (s.end.x() - s.start.x());
      return s.start.y() + t * (s.end.y() - s.start.y());
    }
  return -1.;
}

double value(const RecordedCurve& c, double x)
{
  return c.min + at(c, x) * (c.max - c.min);
}
}

TEST_CASE("A recorded curve spans the box and holds each value", "[recording][curves]")
{
  std::vector<ValueTake::Event> ev{
      {100 * ms, ossia::value{1.f}},
      {500 * ms, ossia::value{3.f}},
      {800 * ms, ossia::value{2.f}}};
  CurveOptions opt;
  opt.simplify = false;
  const auto c = curveOf(ev, 0, 0, 1000 * ms, opt);
  REQUIRE(c);
  CHECK(c->min == 1.);
  CHECK(c->max == 3.);
  REQUIRE(!c->segments.empty());
  CHECK(c->segments.front().start.x() == Approx(0.));
  CHECK(c->segments.back().end.x() == Approx(1.));
  // The first value from the start
  CHECK(value(*c, 0.) == Approx(1.));
  CHECK(value(*c, 0.05) == Approx(1.));
  // Held up to the next
  CHECK(value(*c, 0.45) == Approx(1.));
  CHECK(value(*c, 0.5) == Approx(3.));
  CHECK(value(*c, 0.75) == Approx(3.));
  // The last up to the end
  CHECK(value(*c, 0.9) == Approx(2.));
  CHECK(value(*c, 1.) == Approx(2.));
}

TEST_CASE("A recorded curve of messages goes from one value to the next", "[recording][curves]")
{
  std::vector<ValueTake::Event> ev{{0, ossia::value{0.f}}, {1000 * ms, ossia::value{10.f}}};
  CurveOptions opt;
  opt.simplify = false;
  opt.stepped = false;
  const auto c = curveOf(ev, 0, 0, 1000 * ms, opt);
  REQUIRE(c);
  CHECK(value(*c, 0.5) == Approx(5.));
}

TEST_CASE("Simplifying keeps the shape of a recorded curve", "[recording][curves]")
{
  std::vector<ValueTake::Event> ev;
  for(int i = 0; i <= 1000; i++)
    ev.push_back({i * ms, ossia::value{float(i)}});
  CurveOptions opt;
  opt.stepped = false;
  opt.simplify = true;
  opt.simplificationRatio = 100.;
  const auto c = curveOf(ev, 0, 0, 1000 * ms, opt);
  REQUIRE(c);
  CHECK(c->segments.size() < 50);
  CHECK(value(*c, 0.5) == Approx(500.).margin(10.));
}

TEST_CASE("A vector's axes are recorded curves of their own", "[recording][curves]")
{
  std::vector<ValueTake::Event> ev{
      {0, ossia::value{ossia::vec2f{1.f, 10.f}}},
      {500 * ms, ossia::value{ossia::vec2f{2.f, 20.f}}}};
  CurveOptions opt;
  opt.simplify = false;
  const auto x = curveOf(ev, 0, 0, 1000 * ms, opt);
  const auto y = curveOf(ev, 1, 0, 1000 * ms, opt);
  REQUIRE(x);
  REQUIRE(y);
  CHECK(x->max == 2.);
  CHECK(y->min == 10.);
  CHECK(y->max == 20.);
  CHECK(!curveOf(ev, 2, 0, 1000 * ms, opt));
  CHECK(!curveOf({}, 0, 0, 1000 * ms, opt));
  // A string is no number
  CHECK(!curveOf({{0, ossia::value{std::string{"a"}}}}, 0, 0, 1000 * ms, opt));
}

TEST_CASE("A recorded curve's dates are the box's: values before its start hold from it",
          "[recording][curves]")
{
  std::vector<ValueTake::Event> ev{{200 * ms, ossia::value{4.f}}, {600 * ms, ossia::value{8.f}}};
  CurveOptions opt;
  opt.simplify = false;
  // A box from 400 ms to 800 ms
  const auto c = curveOf(ev, 0, 400 * ms, 800 * ms, opt);
  REQUIRE(c);
  CHECK(value(*c, 0.) == Approx(4.));
  CHECK(value(*c, 0.25) == Approx(4.));
  CHECK(value(*c, 0.75) == Approx(8.));
}

TEST_CASE("Touch: the spans where values came, each until the release after its last", "[recording][curves][touch]")
{
  std::vector<ValueTake::Event> ev{
      {100 * ms, ossia::value{1.f}}, {150 * ms, ossia::value{2.f}},
      // 400 ms later: another touch
      {600 * ms, ossia::value{3.f}}, {700 * ms, ossia::value{4.f}}};
  const auto spans = touchedSpans(ev, 0, 1000 * ms, 200 * ms);
  REQUIRE(spans.size() == 2);
  CHECK(spans[0] == std::pair<int64_t, int64_t>{100 * ms, 350 * ms});
  CHECK(spans[1] == std::pair<int64_t, int64_t>{600 * ms, 900 * ms});
  // Closer than the release: one
  CHECK(touchedSpans(ev, 0, 1000 * ms, 500 * ms).size() == 1);
  // Never past the end of the take
  CHECK(touchedSpans(ev, 0, 800 * ms, 500 * ms).back().second == 800 * ms);
  CHECK(touchedSpans({}, 0, 1000 * ms, 200 * ms).empty());
}
