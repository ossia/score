#include <ossia/dataflow/nodes/spline/spline2d.hpp>
#include <ossia/editor/automation/tinyspline_util.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <vector>

using Catch::Approx;

namespace
{
// Cubic Bezier / Bernstein evaluation for one coordinate.
double bezier3(double p0, double p1, double p2, double p3, double t)
{
  const double u = 1. - t;
  return u * u * u * p0 + 3. * u * u * t * p1 + 3. * u * t * t * p2 + t * t * t * p3;
}
}

TEST_CASE("2D spline with 4 control points is the cubic Bezier of its polygon", "[spline][values]")
{
  // The exact layout ossia::nodes::spline receives from Spline::ProcessModel.
  ossia::spline_data data;
  data.points = {{0., 0.}, {0., 1.}, {1., 1.}, {1., 0.}};

  ts::spline<2> s;
  s.set_points(
      reinterpret_cast<const tsReal*>(data.points.data()), data.points.size());
  REQUIRE(bool(s));

  for(double t : {0., 0.1, 0.25, 0.5, 0.75, 0.9, 1.})
  {
    const auto [x, y] = s.evaluate(t);
    CHECK(x == Approx(bezier3(0., 0., 1., 1., t)).margin(1e-9));
    CHECK(y == Approx(bezier3(0., 1., 1., 0., t)).margin(1e-9));
  }

  // Hand-computed spot values: B(0.5) = (0.5, 0.75), B(0.25) = (0.15625, 0.5625).
  {
    const auto [x, y] = s.evaluate(0.5);
    CHECK(x == Approx(0.5).margin(1e-9));
    CHECK(y == Approx(0.75).margin(1e-9));
  }
  {
    const auto [x, y] = s.evaluate(0.25);
    CHECK(x == Approx(0.15625).margin(1e-9));
    CHECK(y == Approx(0.5625).margin(1e-9));
  }

  // Clamped B-spline interpolates its endpoints.
  {
    const auto [x, y] = s.evaluate(0.);
    CHECK(x == Approx(0.).margin(1e-9));
    CHECK(y == Approx(0.).margin(1e-9));
  }
  {
    const auto [x, y] = s.evaluate(1.);
    CHECK(x == Approx(1.).margin(1e-9));
    CHECK(y == Approx(0.).margin(1e-9));
  }
}

TEST_CASE("2D spline through collinear control points stays on the line", "[spline][values]")
{
  // 6 uniformly spaced points on y = 2x.
  ossia::spline_data data;
  for(int i = 0; i <= 5; i++)
  {
    const double x = i / 5.;
    data.points.push_back({x, 2. * x});
  }

  ts::spline<2> s;
  s.set_points(
      reinterpret_cast<const tsReal*>(data.points.data()), data.points.size());
  REQUIRE(bool(s));

  double last_x = -1.;
  for(int i = 0; i <= 20; i++)
  {
    const double t = i / 20.;
    const auto [x, y] = s.evaluate(t);
    // The convex hull of collinear points is the segment: the curve is on it.
    CHECK(y == Approx(2. * x).margin(1e-9));
    CHECK(x >= -1e-12);
    CHECK(x <= 1. + 1e-12);
    // For a monotonically increasing control polygon x(t) must not go back.
    CHECK(x >= last_x - 1e-12);
    last_x = x;
  }

  const auto [x0, y0] = s.evaluate(0.);
  CHECK(x0 == Approx(0.).margin(1e-9));
  CHECK(y0 == Approx(0.).margin(1e-9));
  const auto [x1, y1] = s.evaluate(1.);
  CHECK(x1 == Approx(1.).margin(1e-9));
  CHECK(y1 == Approx(2.).margin(1e-9));
}

TEST_CASE("2D spline is symmetric for a symmetric control polygon", "[spline][values]")
{
  ossia::spline_data data;
  data.points = {{0., 0.}, {0.25, 1.}, {0.5, -1.}, {0.75, 1.}, {1., 0.}};
  ts::spline<2> fwd;
  fwd.set_points(
      reinterpret_cast<const tsReal*>(data.points.data()), data.points.size());

  ossia::spline_data rev;
  rev.points.assign(data.points.rbegin(), data.points.rend());
  ts::spline<2> bwd;
  bwd.set_points(
      reinterpret_cast<const tsReal*>(rev.points.data()), rev.points.size());

  // Reversing the control polygon reverses the parametrization exactly.
  for(double t : {0., 0.2, 0.4, 0.6, 0.8, 1.})
  {
    const auto [fx, fy] = fwd.evaluate(t);
    const auto [bx, by] = bwd.evaluate(1. - t);
    CHECK(fx == Approx(bx).margin(1e-9));
    CHECK(fy == Approx(by).margin(1e-9));
  }
}

TEST_CASE("3D spline with 4 control points matches the 3D cubic Bezier", "[spline][spline3d][values]")
{
  // Mirrors ossia::nodes::spline3d (used by score-plugin-spline3d).
  const std::array<std::array<double, 3>, 4> pts{
      {{0., 0., 0.}, {1., 2., 3.}, {2., -1., 1.}, {3., 0., -2.}}};

  ts::spline<3> s;
  s.set_points(reinterpret_cast<const tsReal*>(pts.data()), pts.size());
  REQUIRE(bool(s));

  for(double t : {0., 0.25, 0.5, 0.75, 1.})
  {
    const auto [x, y, z] = s.evaluate(t);
    CHECK(x == Approx(bezier3(0., 1., 2., 3., t)).margin(1e-9));
    CHECK(y == Approx(bezier3(0., 2., -1., 0., t)).margin(1e-9));
    CHECK(z == Approx(bezier3(0., 3., 1., -2., t)).margin(1e-9));
  }

  // Spot value at t=0.5: x = 1.5, y = 0.375, z = 1.25.
  const auto [x, y, z] = s.evaluate(0.5);
  CHECK(x == Approx(1.5).margin(1e-9));
  CHECK(y == Approx(0.375).margin(1e-9));
  CHECK(z == Approx(1.25).margin(1e-9));
}
