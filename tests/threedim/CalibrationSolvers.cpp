#include <Gfx/Calibration/EPnP.hpp>
#include <Gfx/Calibration/OffAxis.hpp>
#include <Gfx/Calibration/PlaneFitting.hpp>
#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Calibration/Registration.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <vector>

using Catch::Approx;
namespace procam = score::gfx::procam;

namespace
{
procam::Model groundTruth(double k1 = 0., double k2 = 0.)
{
  procam::Model m;
  m.fx = 1400.;
  m.fy = 1380.;
  m.cx = 960.;
  m.cy = 540.;
  m.k1 = k1;
  m.k2 = k2;
  m.R = (Eigen::AngleAxisd(0.3, Eigen::Vector3d::UnitY())
         * Eigen::AngleAxisd(-0.2, Eigen::Vector3d::UnitX())
         * Eigen::AngleAxisd(0.05, Eigen::Vector3d::UnitZ()))
            .toRotationMatrix();
  m.t = {0.4, -0.2, 4.};
  return m;
}

struct Correspondences
{
  std::vector<procam::Vec3> X;
  std::vector<procam::Vec2> x;
};

Correspondences
observe(const procam::Model& m, int n, bool planar, double noise = 0., uint32_t seed = 3)
{
  std::mt19937 rng{seed};
  std::uniform_real_distribution<double> u(-1., 1.);
  std::normal_distribution<double> g(0., 1.);
  Correspondences c;
  while(int(c.X.size()) < n)
  {
    const procam::Vec3 X(1.5 * u(rng), 1.2 * u(rng), planar ? 0. : u(rng));
    procam::Vec2 px;
    if(!m.project(X, px))
      continue;
    if(noise > 0.)
      px += procam::Vec2(g(rng), g(rng)) * noise;
    c.X.push_back(X);
    c.x.push_back(px);
  }
  return c;
}

std::vector<procam::Vec2> normalised(const procam::Model& m, const std::vector<procam::Vec3>& X)
{
  std::vector<procam::Vec2> xn;
  for(const auto& p : X)
  {
    const procam::Vec3 c = m.R * p + m.t;
    xn.emplace_back(c.x() / c.z(), c.y() / c.z());
  }
  return xn;
}

std::vector<int> iota(int n)
{
  std::vector<int> v(n);
  for(int i = 0; i < n; i++)
    v[i] = i;
  return v;
}

double rotationError(const procam::Mat3& a, const procam::Mat3& b)
{
  return Eigen::AngleAxisd(a * b.transpose()).angle();
}
}

TEST_CASE("procam::solve recovers a known projector", "[procam][calibration]")
{
  const auto truth = groundTruth();
  const auto c = observe(truth, 60, false);
  const auto r = procam::solve(c.X, c.x);
  INFO(r.error);
  REQUIRE(r.ok);
  CHECK(r.rms < 1e-6);
  CHECK(r.model.fx == Approx(truth.fx).epsilon(1e-8));
  CHECK(r.model.fy == Approx(truth.fy).epsilon(1e-8));
  CHECK(r.model.cx == Approx(truth.cx).epsilon(1e-8));
  CHECK(r.model.cy == Approx(truth.cy).epsilon(1e-8));
  CHECK(rotationError(r.model.R, truth.R) < 1e-9);
  CHECK((r.model.t - truth.t).norm() < 1e-8);
  CHECK((r.model.position() - truth.position()).norm() < 1e-8);
}

TEST_CASE("procam::solve with noise stays near the truth", "[procam][calibration]")
{
  const auto truth = groundTruth();
  const auto c = observe(truth, 200, false, 0.3);
  const auto r = procam::solve(c.X, c.x);
  REQUIRE(r.ok);
  CHECK(r.rms == Approx(0.3 * std::sqrt(2.)).epsilon(0.15));
  CHECK(r.model.fx == Approx(truth.fx).epsilon(0.01));
  CHECK((r.model.position() - truth.position()).norm() < 0.02);
}

TEST_CASE("procam::solve recovers the lens distortion", "[procam][calibration]")
{
  const auto truth = groundTruth(-0.12, 0.03);
  const auto c = observe(truth, 150, false);
  procam::Options opt;
  opt.distortion = true;
  const auto r = procam::solve(c.X, c.x, opt);
  INFO(r.error);
  REQUIRE(r.ok);
  CHECK(r.rms < 1e-4);
  CHECK(r.model.k1 == Approx(truth.k1).margin(1e-4));
  CHECK(r.model.k2 == Approx(truth.k2).margin(1e-3));
  CHECK(r.model.fx == Approx(truth.fx).epsilon(1e-5));

  const auto plain = procam::solve(c.X, c.x);
  REQUIRE(plain.ok);
  CHECK(plain.rms > 100. * r.rms);
}

TEST_CASE("procam::solve rejects outliers under RANSAC", "[procam][calibration]")
{
  const auto truth = groundTruth();
  auto c = observe(truth, 100, false, 0.2);
  std::mt19937 rng{11};
  std::uniform_real_distribution<double> px(0., 1920.), py(0., 1080.);
  std::vector<bool> outlier(c.x.size());
  for(std::size_t i = 0; i < c.x.size(); i += 3)
  {
    c.x[i] = {px(rng), py(rng)};
    outlier[i] = true;
  }
  procam::Options opt;
  opt.ransac = true;
  opt.ransacThreshold = 2.;
  const auto r = procam::solve(c.X, c.x, opt);
  INFO(r.error);
  REQUIRE(r.ok);
  REQUIRE(r.mask.size() == c.x.size());
  int wrong = 0;
  for(std::size_t i = 0; i < c.x.size(); i++)
    wrong += bool(r.mask[i]) == outlier[i];
  CHECK(wrong <= 2);
  CHECK(r.model.fx == Approx(truth.fx).epsilon(0.01));
  CHECK((r.model.position() - truth.position()).norm() < 0.03);

  const auto naive = procam::solve(c.X, c.x);
  CHECK((!naive.ok || naive.rms > 50.));
}

TEST_CASE("procam::solve refuses coplanar, too few or non-finite points", "[procam][calibration]")
{
  const auto truth = groundTruth();
  const auto flat = observe(truth, 30, true);
  CHECK_FALSE(procam::solve(flat.X, flat.x).ok);
  auto c = observe(truth, 5, false);
  CHECK_FALSE(procam::solve(c.X, c.x).ok);
  c = observe(truth, 20, false);
  c.X[4].x() = std::numeric_limits<double>::quiet_NaN();
  const auto r = procam::solve(c.X, c.x);
  CHECK_FALSE(r.ok);
  CHECK(r.error.find("finite") != std::string::npos);
}

TEST_CASE("decompose splits K [R | t] into its factors", "[procam][calibration]")
{
  const auto truth = groundTruth();
  procam::Mat3 K;
  K << truth.fx, 0., truth.cx, 0., truth.fy, truth.cy, 0., 0., 1.;
  procam::Mat34 P;
  P.leftCols<3>() = K * truth.R;
  P.col(3) = K * truth.t;
  for(double scale : {1., -3.5, 1e-4})
  {
    procam::Model m;
    REQUIRE(procam::decompose(P * scale, m));
    CHECK(m.fx == Approx(truth.fx).epsilon(1e-10));
    CHECK(m.fy == Approx(truth.fy).epsilon(1e-10));
    CHECK(m.cx == Approx(truth.cx).epsilon(1e-10));
    CHECK(m.cy == Approx(truth.cy).epsilon(1e-10));
    CHECK(rotationError(m.R, truth.R) < 1e-10);
    CHECK((m.t - truth.t).norm() < 1e-9);
  }
}

TEST_CASE("solvePose on a plane recovers the pose", "[procam][calibration]")
{
  const auto truth = groundTruth(-0.05);
  auto known = truth;
  known.R.setIdentity();
  known.t.setZero();
  const auto c = observe(truth, 25, true);
  for(const auto& r : {procam::solvePose(c.X, c.x, known), procam::solvePoseAny(c.X, c.x, known)})
  {
    INFO(r.error);
    REQUIRE(r.ok);
    CHECK(r.rms < 1e-6);
    CHECK(rotationError(r.model.R, truth.R) < 1e-8);
    CHECK((r.model.t - truth.t).norm() < 1e-7);
  }
  const auto four = observe(truth, 4, true, 0., 21);
  const auto r = procam::solvePose(four.X, four.x, known);
  REQUIRE(r.ok);
  CHECK((r.model.t - truth.t).norm() < 1e-6);
}

TEST_CASE("solvePose refuses collinear points", "[procam][calibration]")
{
  const auto truth = groundTruth();
  Correspondences c;
  for(int i = 0; i < 12; i++)
  {
    const procam::Vec3 X(-1. + 0.2 * i, 0.5 - 0.1 * i, 0.3 * i);
    procam::Vec2 px;
    REQUIRE(truth.project(X, px));
    c.X.push_back(X);
    c.x.push_back(px);
  }
  for(const auto& r :
      {procam::solvePose(c.X, c.x, truth), procam::solvePoseAny(c.X, c.x, truth)})
  {
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("collinear") != std::string::npos);
  }
}

TEST_CASE("EPnP alone recovers the pose from 4, 5 and 10 points", "[procam][epnp]")
{
  const auto truth = groundTruth();
  for(int n : {4, 5, 10, 50})
  {
    CAPTURE(n);
    const auto c = observe(truth, n, false, 0., 100 + n);
    const auto xn = normalised(truth, c.X);
    procam::Mat3 R;
    procam::Vec3 t;
    REQUIRE(procam::epnp::solve(c.X, xn, iota(n), R, t));
    CHECK(rotationError(R, truth.R) < 1e-7);
    CHECK((t - truth.t).norm() < 1e-6);
  }
}

TEST_CASE("EPnP with noise stays near the pose", "[procam][epnp]")
{
  const auto truth = groundTruth();
  const auto c = observe(truth, 100, false, 0.5, 7);
  std::vector<procam::Vec2> xn;
  for(const auto& px : c.x)
    xn.push_back(truth.normalise(px));
  procam::Mat3 R;
  procam::Vec3 t;
  REQUIRE(procam::epnp::solve(c.X, xn, iota(100), R, t));
  CHECK(rotationError(R, truth.R) < 2e-3);
  CHECK((t - truth.t).norm() < 0.02);
}

TEST_CASE("EPnP refuses coplanar points", "[procam][epnp]")
{
  const auto truth = groundTruth();
  const auto c = observe(truth, 20, true);
  procam::Mat3 R;
  procam::Vec3 t;
  CHECK_FALSE(procam::epnp::solve(c.X, normalised(truth, c.X), iota(20), R, t));
}

TEST_CASE("solvePoseAny below the 6 points of the DLT goes through EPnP", "[procam][epnp]")
{
  const auto truth = groundTruth();
  for(int n : {4, 5})
  {
    CAPTURE(n);
    const auto c = observe(truth, n, false, 0., 40 + n);
    CHECK_FALSE(procam::solvePose(c.X, c.x, truth).ok);
    const auto r = procam::solvePoseAny(c.X, c.x, truth);
    INFO(r.error);
    REQUIRE(r.ok);
    CHECK(rotationError(r.model.R, truth.R) < 1e-7);
    CHECK((r.model.t - truth.t).norm() < 1e-6);
  }
}

TEST_CASE("pose matrices: inverse of the pose and column-major round trip", "[procam]")
{
  const auto m = groundTruth();
  Eigen::Matrix4d localFromWorld = Eigen::Matrix4d::Identity();
  localFromWorld.topLeftCorner<3, 3>() = m.R;
  localFromWorld.topRightCorner<3, 1>() = m.t;
  CHECK((m.worldFromLocal() * localFromWorld - Eigen::Matrix4d::Identity()).norm() < 1e-12);
  CHECK((m.worldFromLocal().topRightCorner<3, 1>() - m.position()).norm() < 1e-12);
  CHECK((m.R * m.position() + m.t).norm() < 1e-12);

  const auto a = procam::toColumnMajor(m.worldFromLocal());
  CHECK(a[12] == float(m.position().x()));
  CHECK(a[13] == float(m.position().y()));
  CHECK(a[14] == float(m.position().z()));
  CHECK((procam::fromColumnMajor(a) - m.worldFromLocal()).norm() < 1e-6);

  const auto cv = procam::fromColumnMajor(procam::openCvToWorld);
  CHECK((cv * Eigen::Vector4d(1., 2., 3., 1.) - Eigen::Vector4d(1., -2., -3., 1.)).norm() == 0.);
}

TEST_CASE("registration recovers a similarity, and rejects outliers", "[registration]")
{
  namespace reg = score::gfx::registration;
  std::mt19937 rng{5};
  std::uniform_real_distribution<double> u(-2., 2.);
  const Eigen::Matrix3d R
      = (Eigen::AngleAxisd(1.1, Eigen::Vector3d(1., 2., -0.5).normalized())).toRotationMatrix();
  const Eigen::Vector3d t(3., -1., 0.25);
  const double s = 2.5;
  std::vector<Eigen::Vector3d> x, y;
  for(int i = 0; i < 300; i++)
  {
    x.emplace_back(u(rng), u(rng), u(rng));
    y.push_back(s * R * x.back() + t);
  }

  reg::Transform T;
  REQUIRE(reg::umeyama(x, y, T, true));
  CHECK(T.scale == Approx(s).epsilon(1e-12));
  CHECK(Eigen::AngleAxisd(T.R * R.transpose()).angle() < 1e-12);
  CHECK((T.t - t).norm() < 1e-10);
  CHECK(T.R.determinant() == Approx(1.));
  CHECK((T.matrix() * x[0].homogeneous()).head<3>().isApprox(y[0], 1e-12));

  REQUIRE(reg::umeyama(x, y, T, false));
  CHECK(T.scale == 1.);
  CHECK(Eigen::AngleAxisd(T.R * R.transpose()).angle() < 1e-12);

  for(int i = 0; i < 300; i += 4)
    y[i] += Eigen::Vector3d(u(rng), u(rng), u(rng));
  const auto r = reg::ransac(x, y, 1e-6, true);
  REQUIRE(r.ok);
  CHECK(r.inliers == 225);
  CHECK(r.transform.scale == Approx(s).epsilon(1e-10));
  CHECK((r.transform.t - t).norm() < 1e-9);

  std::vector<Eigen::Vector3d> line;
  for(int i = 0; i < 10; i++)
    line.emplace_back(i, 2. * i, -i);
  CHECK_FALSE(reg::umeyama(line, line, T, true));
}

TEST_CASE("planes: least-squares plane and sequential RANSAC", "[planes]")
{
  namespace planes = score::gfx::planes;
  std::mt19937 rng{2};
  std::uniform_real_distribution<double> u(-1., 1.);
  std::normal_distribution<double> g(0., 0.002);
  std::vector<Eigen::Vector3d> pts;
  const Eigen::Vector3d floorN(0., 1., 0.), wallN = Eigen::Vector3d(1., 0., 0.3).normalized();
  for(int i = 0; i < 600; i++)
    pts.emplace_back(2. * u(rng), -1. + g(rng), 2. * u(rng));
  for(int i = 0; i < 400; i++)
  {
    const Eigen::Vector3d a = wallN.unitOrthogonal(), b = wallN.cross(a);
    pts.push_back(wallN * 1.5 + a * u(rng) + b * u(rng) + wallN * g(rng));
  }

  planes::Plane p;
  REQUIRE(planes::fitPlane(pts, iota(600), p));
  CHECK(std::abs(p.n.dot(floorN)) == Approx(1.).margin(1e-4));
  CHECK(std::abs(p.d) == Approx(1.).margin(1e-3));

  planes::Options opt;
  opt.threshold = 0.01;
  const auto found = planes::fitPlanes(pts, Eigen::Vector3d::Zero(), opt);
  REQUIRE(found.size() >= 2);
  CHECK(found[0].n.dot(floorN) == Approx(1.).margin(1e-3));
  CHECK(found[0].d == Approx(1.).margin(2e-3));
  CHECK(found[0].inliers >= 590);
  CHECK(found[1].n.dot(-wallN) == Approx(1.).margin(1e-3));
  CHECK(found[1].d == Approx(1.5).margin(2e-3));
  CHECK(found[1].inliers >= 390);
  for(const auto& q : found)
    CHECK(q.d > 0.);
}

TEST_CASE("kooima: the frustum of a centred and of an offset eye", "[offaxis]")
{
  namespace oa = score::gfx::offaxis;
  const oa::Vec3 pa(-1., -1., -2.), pb(1., -1., -2.), pc(-1., 1., -2.);

  const auto c = oa::kooima(pa, pb, pc, oa::Vec3::Zero());
  REQUIRE(c.ok);
  CHECK(c.l == Approx(-0.5));
  CHECK(c.r == Approx(0.5));
  CHECK(c.b == Approx(-0.5));
  CHECK(c.t == Approx(0.5));
  CHECK(c.d == Approx(2.));

  const oa::Vec3 pe(0.5, 0.25, 0.);
  const auto o = oa::kooima(pa, pb, pc, pe);
  REQUIRE(o.ok);
  CHECK(o.l == Approx(-0.75));
  CHECK(o.r == Approx(0.25));
  CHECK(o.b == Approx(-0.625));
  CHECK(o.t == Approx(0.375));
  CHECK((o.worldFromEye(pe) * o.view(pe) - Eigen::Matrix4d::Identity()).norm() < 1e-12);

  const double n = 0.1, f = 50.;
  const Eigen::Matrix4d PV = o.projection(n, f) * o.view(pe);
  auto ndc = [&](const oa::Vec3& p) {
    const Eigen::Vector4d q = PV * p.homogeneous();
    return Eigen::Vector3d(q.head<3>() / q.w());
  };
  CHECK((ndc(pa).head<2>() - Eigen::Vector2d(-1., -1.)).norm() < 1e-12);
  CHECK((ndc(pb).head<2>() - Eigen::Vector2d(1., -1.)).norm() < 1e-12);
  CHECK((ndc(pc).head<2>() - Eigen::Vector2d(-1., 1.)).norm() < 1e-12);
  CHECK(ndc(pe - n * o.vn).z() == Approx(1.));
  CHECK(ndc(pe - f * o.vn).z() == Approx(-1.));
  CHECK(ndc(pa).z() < 1.);
  CHECK(ndc(pa).z() > -1.);

  CHECK(o.projection(0., f) == Eigen::Matrix4d::Identity());
  CHECK(o.projection(f, n) == Eigen::Matrix4d::Identity());
}
