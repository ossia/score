// The Point2D View's axis mapping.
//
// The view refused any range whose max was not greater than its min --
// `if(scalex < 0.000001f || scaley < 0.000001f) return;` -- so setting Min Y
// above Max Y, the natural way to point Y downwards, drew nothing at all.
// A range the wrong way round is a legitimate range; only a range of no width
// is not.

#include <UI/2DView.hpp>

#include <catch2/catch_all.hpp>

#include <cmath>

using uo::axisRatio;
using uo::axisUsable;

TEST_CASE("an axis maps its range onto [0;1]")
{
  CHECK(axisRatio(0., 0., 1.) == Catch::Approx(0.));
  CHECK(axisRatio(1., 0., 1.) == Catch::Approx(1.));
  CHECK(axisRatio(0.25, 0., 1.) == Catch::Approx(0.25));

  // A range that does not start at zero.
  CHECK(axisRatio(-10., -10., 30.) == Catch::Approx(0.));
  CHECK(axisRatio(10., -10., 30.) == Catch::Approx(0.5));
}

TEST_CASE("an axis whose min is above its max reads the other way round")
{
  // Min Y = 1, Max Y = 0: the way to point Y downwards.
  CHECK(axisRatio(1., 1., 0.) == Catch::Approx(0.));
  CHECK(axisRatio(0., 1., 0.) == Catch::Approx(1.));
  CHECK(axisRatio(0.25, 1., 0.) == Catch::Approx(0.75));

  // Mirror of the same range, point for point.
  for(double v : {0., 0.1, 0.5, 0.9, 1.})
  {
    INFO("value " << v);
    CHECK(axisRatio(v, 1., 0.) == Catch::Approx(1. - axisRatio(v, 0., 1.)));
  }
}

TEST_CASE("only an axis of no width is unusable")
{
  CHECK(axisUsable(0., 1.));
  CHECK(axisUsable(1., 0.));   // inverted, and drawn
  CHECK(axisUsable(-5., -20.));
  CHECK_FALSE(axisUsable(1., 1.));
  CHECK_FALSE(axisUsable(1., 1. + 1e-9));
}

TEST_CASE("the origin is in view only when the range contains it")
{
  const auto inView = [](double min, double max) {
    const auto r = axisRatio(0., min, max);
    return r >= 0. && r <= 1.;
  };

  CHECK(inView(0., 1.));
  CHECK(inView(-1., 1.));
  CHECK(inView(1., -1.)); // inverted, still contains zero
  CHECK_FALSE(inView(1., 2.));
  CHECK_FALSE(inView(-2., -1.));
  CHECK_FALSE(inView(2., 1.)); // inverted, and zero is outside it
}
