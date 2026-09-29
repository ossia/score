// Durations a script gives beyond what a TimeVal holds are refused rather than
// wrapped around: std::llround is undefined past the int64 range.
#include <JS/Qml/ParseDuration.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>

TEST_CASE("durations beyond the int64 range of flicks are refused", "[js]")
{
  CHECK_FALSE(JS::parseDuration("1e30"));
  CHECK_FALSE(JS::parseDuration("9.3e18"));
  CHECK_FALSE(JS::parseDuration("1e300 s"));
  CHECK_FALSE(JS::parseDuration("1e12 h"));
  CHECK_FALSE(JS::parseDuration("99999999999999:00:00"));
  CHECK_FALSE(JS::parseDuration("inf"));
  CHECK_FALSE(JS::parseDuration("nan"));
  CHECK_FALSE(JS::parseDuration("nan s"));
}

TEST_CASE("durations up to the int64 range of flicks are kept", "[js]")
{
  auto d = JS::parseDuration("9.2e18");
  REQUIRE(d);
  CHECK(d->impl == int64_t(9.2e18));

  // About a week in flicks, well inside the range
  d = JS::parseDuration("168 h");
  REQUIRE(d);
  CHECK(d->impl == int64_t(168) * 3600 * ossia::flicks_per_second<int64_t>);

  d = JS::parseDuration("0");
  REQUIRE(d);
  CHECK(d->impl == 0);
}
