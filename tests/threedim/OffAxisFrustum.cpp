// Generalized perspective projection (Kooima 2008) against the analytic
// frustum of a CAVE wall, and the tight viewer frustum of a projector's XYZ map.

#include <Gfx/Calibration/OffAxis.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace oa = score::gfx::offaxis;
using Catch::Approx;
using oa::Vec3;

namespace
{
Eigen::Vector3d ndc(const Eigen::Matrix4d& PV, const Vec3& p)
{
  const Eigen::Vector4d c = PV * Eigen::Vector4d(p.x(), p.y(), p.z(), 1.);
  return c.head<3>() / c.w();
}

std::vector<float> wallMap(const Vec3& pos, const Eigen::Matrix3d& wR, double f, int W, int H,
                           bool curved)
{
  std::vector<float> xyz(std::size_t(W) * H * 4, 0.f);
  for(int v = 0; v < H; v++)
    for(int u = 0; u < W; u++)
    {
      const Vec3 d = wR * Vec3((u - (W - 1) / 2.) / f, (v - (H - 1) / 2.) / f, 1.);
      Vec3 p;
      if(!curved)
        p = pos + ((-2. - pos.z()) / d.z()) * d;
      else
      {
        // Inside of a cylinder of radius 3 around the y axis.
        const double a = d.x() * d.x() + d.z() * d.z();
        const double b = 2. * (pos.x() * d.x() + pos.z() * d.z());
        const double c = pos.x() * pos.x() + pos.z() * pos.z() - 9.;
        p = pos + ((-b + std::sqrt(b * b - 4. * a * c)) / (2. * a)) * d;
      }
      float* q = xyz.data() + (std::size_t(v) * W + u) * 4;
      q[0] = float(p.x());
      q[1] = float(p.y());
      q[2] = float(p.z());
      q[3] = 1.f;
    }
  return xyz;
}
}

TEST_CASE("Kooima: CAVE left wall against the analytic frustum", "[offaxis]")
{
  const Vec3 pa(-1.5, 0., 1.5), pb(-1.5, 0., -1.5), pc(-1.5, 3., 1.5), pe(0.3, 1.6, 0.2);
  const auto f = oa::kooima(pa, pb, pc, pe);
  REQUIRE(f.ok);
  CHECK((f.vr - Vec3(0., 0., -1.)).norm() < 1e-12);
  CHECK((f.vu - Vec3(0., 1., 0.)).norm() < 1e-12);
  CHECK((f.vn - Vec3(1., 0., 0.)).norm() < 1e-12);
  CHECK(f.d == Approx(1.8));
  CHECK(f.l == Approx(-1.3 / 1.8));
  CHECK(f.r == Approx(1.7 / 1.8));
  CHECK(f.b == Approx(-1.6 / 1.8));
  CHECK(f.t == Approx(1.4 / 1.8));

  const double n = 0.1, fa = 100.;
  const Eigen::Matrix4d PV = f.projection(n, fa) * f.view(pe);
  const Vec3 pd = pb + (pc - pa);
  CHECK((ndc(PV, pa).head<2>() - Eigen::Vector2d(-1., -1.)).norm() < 1e-12);
  CHECK((ndc(PV, pb).head<2>() - Eigen::Vector2d(1., -1.)).norm() < 1e-12);
  CHECK((ndc(PV, pc).head<2>() - Eigen::Vector2d(-1., 1.)).norm() < 1e-12);
  CHECK((ndc(PV, pd).head<2>() - Eigen::Vector2d(1., 1.)).norm() < 1e-12);
  const Vec3 p(-1.5, 2.1, -0.4);
  const auto q = ndc(PV, p);
  CHECK(q.x() == Approx(2. * (1.5 + 0.4) / 3. - 1.));
  CHECK(q.y() == Approx(2. * 2.1 / 3. - 1.));
  CHECK(ndc(PV, pe + n * (-f.vn)).z() == Approx(1.));
  CHECK(ndc(PV, pe + fa * (-f.vn)).z() == Approx(-1.));

  const auto s = oa::kooima(Vec3(-1, -1, -1), Vec3(1, -1, -1), Vec3(-1, 1, -1), Vec3::Zero());
  CHECK(s.l == Approx(-1.));
  CHECK(s.r == Approx(1.));
  CHECK(s.t == Approx(1.));
  CHECK(s.projection(1., 10.)(0, 0) == Approx(1.));
  CHECK(s.projection(1., 10.)(0, 2) == Approx(0.).margin(1e-12));
}

TEST_CASE("Viewer frustum of a projector map: at the projector it is the projector", "[offaxis]")
{
  const int W = 400, H = 250;
  const double fpx = 380.;
  const Vec3 pos(0.4, 1.3, 0.5);
  const Eigen::Matrix3d flip = Vec3(1., -1., -1.).asDiagonal();
  const auto xyz = wallMap(pos, flip, fpx, W, H, false);

  // From the projector's own centre, the frustum through the pixel centres
  // spans (W - 1) / f horizontally, and needs about W x H pixels.
  const auto r = oa::fitViewerFrustum(xyz, W, H, pos, 0.01, 1., 0.5);
  INFO(r.error);
  REQUIRE(r.ok);
  CHECK(r.planar);
  CHECK(r.planeRms < 1e-4);
  INFO("size " << r.width << " x " << r.height);
  CHECK(std::abs(r.width - W) <= 2);
  CHECK(std::abs(r.height - H) <= 2);
  CHECK(r.frustum.l == Approx(-(W - 1) / 2. / fpx));
  CHECK(r.frustum.t == Approx((H - 1) / 2. / fpx));
  for(const Vec3& p : {r.pa, r.pb, r.pc})
    CHECK(p.z() == Approx(-2.).margin(1e-6));

  // From a viewer elsewhere every lit point is inside, and the bounds are tight.
  const Vec3 eye(-0.5, 1.7, 0.9);
  const auto s = oa::fitViewerFrustum(xyz, W, H, eye, 0.01, 1.);
  REQUIRE(s.ok);
  const auto f = oa::kooima(s.pa, s.pb, s.pc, eye);
  REQUIRE(f.ok);
  CHECK(f.l == Approx(s.frustum.l).margin(1e-9));
  CHECK(f.t == Approx(s.frustum.t).margin(1e-9));
  const Eigen::Matrix4d PV = f.projection(0.1, 100.) * f.view(eye);
  double maxAbs = 0.;
  for(std::size_t i = 0; i < xyz.size(); i += 4)
  {
    const auto q = ndc(PV, Vec3(xyz[i], xyz[i + 1], xyz[i + 2]));
    maxAbs = std::max({maxAbs, std::abs(q.x()), std::abs(q.y())});
  }
  CHECK(maxAbs <= 1. + 1e-9);
  CHECK(maxAbs > 0.995);
}

TEST_CASE("Viewer frustum of a curved screen", "[offaxis]")
{
  const int W = 300, H = 200;
  const Eigen::Matrix3d flip = Vec3(1., -1., -1.).asDiagonal();
  const auto xyz = wallMap(Vec3(0., 1.5, 0.), flip, 200., W, H, true);
  const auto r = oa::fitViewerFrustum(xyz, W, H, Vec3(0., 1.6, 0.4), 0.01, 1.);
  INFO(r.error);
  REQUIRE(r.ok);
  CHECK_FALSE(r.planar);
  CHECK(r.width > 0);
  const auto f = oa::kooima(r.pa, r.pb, r.pc, Vec3(0., 1.6, 0.4));
  CHECK(f.r == Approx(r.frustum.r).margin(1e-9));

  // A surface around the viewer cannot have one frustum.
  std::vector<float> ring(360 * 2 * 4, 0.f);
  for(int y = 0; y < 2; y++)
    for(int a = 0; a < 360; a++)
    {
      float* q = ring.data() + (y * 360 + a) * 4;
      q[0] = float(3. * std::cos(a * M_PI / 180.));
      q[1] = float(1. + y);
      q[2] = float(3. * std::sin(a * M_PI / 180.) + 0.3);
      q[3] = 1.f;
    }
  const auto around = oa::fitViewerFrustum(ring, 360, 2, Vec3(0., 1.5, 0.), 0.01, 1.);
  INFO(around.error);
  CHECK_FALSE(around.ok);
}
