// NaN and infinity tests under -ffast-math, which this file is built with:
// the compiler takes every double to be finite there, and folds the standard
// tests and, for clang, tests of the bits too.

#include <ossia/math/safe_math.hpp>

#include <catch2/catch_test_macros.hpp>
#include <halp/safe_math.hpp>

#include <cstdlib>
#include <limits>

static_assert(__FINITE_MATH_ONLY__, "this test is meant to be built with -ffast-math");

namespace
{
//! A value the compiler cannot know: read from text at run time
[[gnu::noinline]] double parsed(const char* text)
{
  return std::strtod(text, nullptr);
}
}

TEST_CASE("NaN and infinity are told apart under -ffast-math", "[math][fast-math]")
{
  const double nan = parsed("nan"), inf = parsed("inf"), minf = parsed("-inf");
  const double one = parsed("1"), tiny = parsed("4.9e-324"), big = parsed("1.7e308");

  CHECK(ossia::safe_isnan(nan));
  CHECK(!ossia::safe_isnan(inf));
  CHECK(!ossia::safe_isnan(one));
  CHECK(ossia::safe_isinf(inf));
  CHECK(ossia::safe_isinf(minf));
  CHECK(!ossia::safe_isinf(nan));
  CHECK(!ossia::safe_isinf(big));
  CHECK(!ossia::safe_isfinite(nan));
  CHECK(!ossia::safe_isfinite(inf));
  CHECK(!ossia::safe_isfinite(minf));
  CHECK(ossia::safe_isfinite(one));
  CHECK(ossia::safe_isfinite(tiny));
  CHECK(ossia::safe_isfinite(big));
  // A float promoted to a double keeps what it is
  CHECK(!ossia::safe_isfinite(float(inf)));
  CHECK(ossia::safe_isnan(float(nan)));

  CHECK(halp::safe_isnan(nan));
  CHECK(!halp::safe_isnan(inf));
  CHECK(halp::safe_isinf(inf));
  CHECK(halp::safe_isinf(minf));
  CHECK(!halp::safe_isinf(nan));
  CHECK(!halp::safe_isfinite(nan));
  CHECK(!halp::safe_isfinite(inf));
  CHECK(halp::safe_isfinite(one));
  CHECK(halp::safe_isfinite(tiny));
}
