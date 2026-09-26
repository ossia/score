#include <JS/Qml/ParseDuration.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
int64_t flicks(double seconds)
{
  return int64_t(seconds * ossia::flicks_per_second<double>);
}
int64_t parse(const char* s)
{
  auto t = JS::parseDuration(QString::fromUtf8(s));
  REQUIRE(t);
  return t->impl;
}
}

TEST_CASE("script durations: numbers are flicks", "[js][duration]")
{
  CHECK(parse("1411200000") == flicks(2.));
  CHECK(parse(" 705600000 ") == flicks(1.));
  CHECK(parse("0") == 0);
}

TEST_CASE("script durations: numbers with a unit", "[js][duration]")
{
  CHECK(parse("500ms") == flicks(0.5));
  CHECK(parse("2s") == flicks(2.));
  CHECK(parse("1.5 s") == flicks(1.5));
  CHECK(parse("2min") == flicks(120.));
  CHECK(parse("1h") == flicks(3600.));
}

TEST_CASE("script durations: clock times", "[js][duration]")
{
  CHECK(parse("0:05") == flicks(5.));
  CHECK(parse("00:00:08") == flicks(8.));
  CHECK(parse("1:02:03.250") == flicks(3723.25));
  CHECK(parse("25:00:00") == flicks(25 * 3600.));
}

TEST_CASE("script durations: refused input", "[js][duration]")
{
  for(const char* s : {"", "abc", "-1", "-2s", "1:60", "1:2:3:4", "2 parsecs", "nan"})
  {
    CAPTURE(s);
    CHECK_FALSE(JS::parseDuration(QString::fromUtf8(s)));
  }
}
