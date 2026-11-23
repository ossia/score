// Calibration maths on hostile or degenerate input: NaN through the
// photometric tables, invalid response fits, LED cabinet CSVs with huge
// rasters, duplicated XYZ samples, skewed screens, the slope constraint
// without links, points behind the projector, distortion in the pose solve,
// and RANSAC stopping once confident.

#include <Gfx/Calibration/Blending.hpp>
#include <Gfx/Calibration/EPnP.hpp>
#include <Gfx/Calibration/LedMapping.hpp>
#include <Gfx/Calibration/OffAxis.hpp>
#include <Gfx/Calibration/Photometric.hpp>
#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Calibration/Registration.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace ph = score::gfx::photometric;
using Catch::Approx;

namespace
{
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

ph::ColourMixing gridMixing(int N, auto&& truth)
{
  ph::ColourMixing m;
  for(int c = 0; c < 3; c++)
    for(int k = 0; k < N; k++)
      m.axis[c].push_back(std::pow(k / double(N - 1), 2.2));
  for(int b = 0; b < N; b++)
    for(int g = 0; g < N; g++)
      for(int r = 0; r < N; r++)
        m.shown.push_back(truth({m.axis[0][r], m.axis[1][g], m.axis[2][b]}));
  return m;
}

std::array<double, 3> dlp(const std::array<double, 3>& l)
{
  const double m = 0.5 * std::min({l[0], l[1], l[2]});
  return {l[0] + m, l[1] + m, l[2] + m};
}
}

TEST_CASE("photometric tables map NaN to a finite value", "[photometric][hardening]")
{
  ph::CameraResponse crf;
  crf.levels = 4;
  for(auto& g : crf.g)
    g = {-3., -2., -1., 0.};
  CHECK(std::isfinite(crf.linear(0, kNaN)));
  CHECK(std::isfinite(crf.linear(1, kInf)));
  CHECK(std::isfinite(crf.linear(2, -kInf)));

  const ph::MonotoneCubic cubic({0., 0.5, 1.}, {0., 0.25, 1.});
  CHECK(cubic(kNaN) == 0.);
  CHECK(cubic(kInf) == 1.);
  CHECK(cubic(-kInf) == 0.);
  const ph::MonotoneCubic uneven({0., 0.5, 1.}, {0., 1.});
  CHECK(std::isfinite(uneven(0.75)));

  const auto m = gridMixing(3, dlp);
  const auto f = m.forward({kNaN, 0.5, kInf});
  for(double v : f)
    CHECK(std::isfinite(v));
  const auto inv = ph::MixingInverse::build(m, 9);
  REQUIRE(inv.valid());
  for(const auto& e : {std::array<double, 3>{kNaN, 0.2, 0.2}, {kInf, 0.1, 0.1}, {-kInf, kNaN, kInf}})
  {
    const auto l = inv(e);
    for(double v : l)
      CHECK(std::isfinite(v));
  }
  std::array<double, 3> l;
  CHECK_FALSE(m.solve({kNaN, 0.1, 0.1}, l));
  const auto c = m.closest({kNaN, 0.2, 0.2});
  for(double v : c)
    CHECK(std::isfinite(v));
}

TEST_CASE("compensationMatrix leaves the identity when it fails", "[photometric][hardening]")
{
  Eigen::Matrix3d M;
  M.setConstant(42.);
  CHECK_FALSE(ph::compensationMatrix(Eigen::Matrix3d::Identity(), {kInf, 1., 1.}, M));
  CHECK(M == Eigen::Matrix3d::Identity());
  M.setConstant(42.);
  CHECK_FALSE(ph::compensationMatrix(Eigen::Matrix3d::Zero(), {1., 1., 1.}, M));
  CHECK(M == Eigen::Matrix3d::Identity());
}

TEST_CASE("fitChannelResponse flags what it cannot fit", "[photometric][hardening]")
{
  const auto good = ph::fitChannelResponse({0.25, 0.5, 1.}, {0.05, 0.2, 1.});
  CHECK(good.valid);
  CHECK(good(0.5) == Approx(0.2).margin(1e-6));

  for(const auto& [x, m] :
      {std::pair<std::vector<double>, std::vector<double>>{{0.5, 1.}, {0.2, 0.}},
       {{0.5, 1.}, {0.2, -1.}},
       {{0.25, 0.5, 1.}, {0.1}},
       {{0.5, 1.}, {kNaN, 1.}},
       {{}, {}},
       {{1., 0.5}, {1., 0.5}}})
  {
    const auto r = ph::fitChannelResponse(x, m);
    CHECK_FALSE(r.valid);
    CHECK(r(0.3) == Approx(0.3));
    CHECK(r.inverse(0.3) == Approx(0.3).margin(1e-6));
  }
}

TEST_CASE("solveCameraResponse rejects short codes and skips zero exposures", "[photometric][hardening]")
{
  const std::vector<double> lnDt{std::log(0.), std::log(0.5), std::log(1.)};
  CHECK(ph::solveCameraResponse({1, 2}, 4, lnDt, 16, 10.).empty());
  CHECK(ph::solveCameraResponse({}, 0, lnDt, 1, 10.).empty());

  std::vector<int> Z;
  const int samples = 40;
  for(int i = 0; i < samples; i++)
  {
    const double E = 0.02 + 0.9 * i / double(samples);
    for(double dt : {0., 0.5, 1.})
      Z.push_back(std::clamp(int(std::lround(std::sqrt(E * dt) * 15.)), 0, 15));
  }
  const auto g = ph::solveCameraResponse(Z, samples, lnDt, 16, 10.);
  REQUIRE(g.size() == 16);
  for(double v : g)
    CHECK(std::isfinite(v));
  CHECK(g[8] == 0.);
  for(int z = 1; z < 16; z++)
    CHECK(g[z] > g[z - 1]);
}

TEST_CASE("the gamut table bounds its grid", "[photometric][hardening]")
{
  const auto m5 = gridMixing(5, dlp);
  const auto inv = ph::MixingInverse::build(m5);
  REQUIRE(inv.valid());

  CHECK_FALSE(ph::MixingInverse::build(gridMixing(18, dlp)).valid());
  CHECK_FALSE(ph::MixingInverse::build(m5, 1000).valid());
  auto broken = m5;
  broken.shown[7][1] = kNaN;
  CHECK_FALSE(ph::MixingInverse::build(broken).valid());
  broken = m5;
  std::swap(broken.axis[0][1], broken.axis[0][2]);
  CHECK_FALSE(ph::MixingInverse::build(broken).valid());
}

TEST_CASE("a dark colour just out of gamut keeps its hue", "[photometric][hardening]")
{
  constexpr int N = 5, K = 33;
  ph::ColourMixing m;
  for(int c = 0; c < 3; c++)
    for(int k = 0; k < N; k++)
      m.axis[c].push_back(k / double(N - 1));
  for(int b = 0; b < N; b++)
    for(int g = 0; g < N; g++)
      for(int r = 0; r < N; r++)
        m.shown.push_back({0.1 * m.axis[0][r], m.axis[1][g], m.axis[2][b]});
  const double D = 1.06496;
  const auto inv = ph::MixingInverse::build(m, K, D);
  REQUIRE(inv.valid());
  auto node = [&](int j) { return D * j * j / double((K - 1) * (K - 1)); };
  const std::array<double, 3> e{node(10), node(7), 0.};
  REQUIRE(e[0] > 0.1);
  const auto shown = m.forward(inv(e));
  CHECK(shown[0] <= 0.1 + 1e-6);
  CHECK(shown[1] / shown[0] == Approx(e[1] / e[0]).epsilon(0.005));
}

TEST_CASE("LED cabinets: huge or non-finite CSV values are rejected", "[led][hardening]")
{
  namespace led = score::gfx::led;
  const std::string corners = " 0 0 0  1 0 0  0 -1 0  1 -1 0";
  std::string err;
  CHECK(led::parseModules("0 0 100000 100000" + corners, &err).empty());
  CHECK_FALSE(err.empty());
  CHECK(led::parseModules("1e300 0 10 10" + corners).empty());
  CHECK(led::parseModules("nan 0 10 10" + corners).empty());
  CHECK(led::parseModules("0 0 10 10 nan 0 0  1 0 0  0 -1 0  1 -1 0").empty());

  const auto distant = led::parseModules("65536 65536 65536 65536" + corners);
  REQUIRE(distant.size() == 1);
  const auto sz = led::rasterSize(distant);
  CHECK(sz[0] == led::maxRasterSide);
  CHECK(sz[1] == led::maxRasterSide);
  CHECK(led::rasterize(distant, sz[0], sz[1]).empty());
  CHECK(led::rasterize(distant, 100000, 10).empty());

  const auto offscreen = led::parseModules("-65532 -65532 65536 65536" + corners);
  REQUIRE(offscreen.size() == 1);
  const auto map = led::rasterize(offscreen, 8, 8);
  REQUIRE(map.size() == 8 * 8 * 4);
  CHECK(map[3] == 1.f);
  CHECK(map[4 * 8 * 4 + 3] == 0.f);

  const auto code = led::BlinkCode::make(INT_MAX);
  CHECK(code.bits <= 25);
  CHECK(code.count == 1 << 24);

  const auto small = led::BlinkCode::make(3);
  std::vector<std::vector<float>> frames(small.frames(), std::vector<float>(16, 1.f));
  frames[3].resize(4);
  CHECK(led::decode(small, frames, 4, 4, {}).empty());
  frames[3].resize(16, 0.f);
  std::vector<float> positions(4);
  const auto without = led::decode(small, frames, 4, 4, {});
  const auto with = led::decode(small, frames, 4, 4, {}, &positions);
  REQUIRE(with.size() == without.size());
  for(const auto& f : with)
    CHECK_FALSE(f.hasWorld);
}

TEST_CASE("viewer frustum: duplicated XYZ samples keep the render size sane", "[offaxis][hardening]")
{
  namespace oa = score::gfx::offaxis;
  const int W = 256, H = 128;
  std::vector<float> xyz(std::size_t(W) * H * 4);
  for(int v = 0; v < H; v++)
    for(int u = 0; u < W; u++)
    {
      float* p = xyz.data() + (std::size_t(v) * W + u) * 4;
      p[0] = float((u / 4) * 0.02 - 1.28);
      p[1] = float(0.64 - (v / 4) * 0.02);
      p[2] = -2.f;
      p[3] = 1.f;
    }
  const auto f = oa::fitViewerFrustum(xyz, W, H, oa::Vec3::Zero(), 0.01, 1.);
  INFO(f.error);
  REQUIRE(f.ok);
  CHECK(f.width < 1024);
  CHECK(f.height < 512);
  CHECK(f.width >= W / 4);

  std::vector<float> same(std::size_t(W) * H * 4, 0.f);
  for(std::size_t i = 0; i < same.size(); i += 4)
  {
    same[i + 2] = -2.f;
    same[i + 3] = 1.f;
  }
  same[2] = -2.5f;
  same[W * 4 + 2] = -3.f;
  same[W * 4 * 2 + 4 * 5] = 0.1f;
  const auto g = oa::fitViewerFrustum(same, W, H, oa::Vec3::Zero(), 0.01, 1e300);
  if(g.ok)
  {
    CHECK(g.width <= 16384);
    CHECK(g.height <= 16384);
  }
  else
    CHECK_FALSE(g.error.empty());

  CHECK_FALSE(oa::fitViewerFrustum(std::vector<float>(16), W, H, oa::Vec3::Zero(), 0.01, 1.).ok);
}

TEST_CASE("kooima: skewed corners give an orthonormal view", "[offaxis][hardening]")
{
  namespace oa = score::gfx::offaxis;
  const oa::Vec3 pa(-1., -1., -2.), pb(1., -1., -2.), pc(-0.6, 1., -2.3);
  const auto f = oa::kooima(pa, pb, pc, oa::Vec3::Zero());
  REQUIRE(f.ok);
  const Eigen::Matrix3d R = f.view(oa::Vec3::Zero()).topLeftCorner<3, 3>();
  CHECK((R * R.transpose() - Eigen::Matrix3d::Identity()).norm() < 1e-12);
  CHECK(R.determinant() == Approx(1.));
  CHECK(f.vr.dot(f.vu) == Approx(0.).margin(1e-12));
  CHECK(f.vn.dot((pb - pa).cross(pc - pa).normalized()) == Approx(1.));
}

TEST_CASE("constrainSlope holds the bound along a serpentine without links", "[blend][hardening]")
{
  namespace blend = score::gfx::blend;
  const int W = 64, H = 64;
  blend::Projector p;
  p.width = W;
  p.height = H;
  p.xyz.assign(std::size_t(W) * H * 4, 0.f);
  auto open = [&](int x, int y) {
    if(x % 2 == 0)
      return true;
    const bool gapAtBottom = (x / 2) % 2 == 0;
    return gapAtBottom ? y == H - 1 : y == 0;
  };
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
      if(open(x, y))
      {
        float* q = p.xyz.data() + (std::size_t(y) * W + x) * 4;
        q[0] = x * 0.01f;
        q[1] = y * 0.01f;
        q[3] = 1.f;
      }
  std::vector<blend::Projector> P{p};
  std::vector<std::vector<float>> f(1, std::vector<float>(std::size_t(W) * H, 1.f));
  f[0][std::size_t(H - 1) * W + (W - 2)] = 1e-3f;
  const std::vector<std::vector<blend::detail::Link>> links(1);
  blend::Options opt;
  const double slope = 2.;
  blend::detail::constrainSlope(f, P, links, slope, opt, false);

  double worst = 0.;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const int a = y * W + x;
      if(!p.valid(a))
        continue;
      for(int dy = -1; dy <= 1; dy++)
        for(int dx = -1; dx <= 1; dx++)
        {
          const int nx = x + dx, ny = y + dy;
          if(nx < 0 || ny < 0 || nx >= W || ny >= H || (dx == 0 && dy == 0))
            continue;
          const int b = ny * W + nx;
          if(!p.valid(b))
            continue;
          const double d = (p.point(a) - p.point(b)).norm();
          worst = std::max(
              worst, std::abs(std::log(f[0][a]) - std::log(f[0][b])) - slope * d);
        }
    }
  CHECK(worst < 1e-4);
  CHECK(f[0][0] == Approx(1.f));
  CHECK(f[0][std::size_t(H - 1) * W + (W - 2)] == Approx(1e-3f));
}

TEST_CASE("blend::compute refuses an XYZ map of the wrong size", "[blend][hardening]")
{
  namespace blend = score::gfx::blend;
  std::vector<blend::Projector> P(1);
  P[0].width = 4;
  P[0].height = 4;
  P[0].xyz.assign(8, 1.f);
  const auto r = blend::compute(P, {});
  CHECK(r.status.rfind("Failed", 0) == 0);
  REQUIRE(r.alpha.size() == 1);
  CHECK(r.alpha[0].size() == 16);
  CHECK(blend::sampleBilinear({}, 0, 0, 0.5, 0.5) == 0.f);
}

TEST_CASE("pinhole: a point behind the projector is not an inlier", "[procam][hardening]")
{
  namespace procam = score::gfx::procam;
  procam::Model m;
  m.fx = m.fy = 800.;
  m.cx = 320.;
  m.cy = 240.;
  std::vector<procam::Vec3> X{{0., 0., 2.}, {0.1, 0., 2.}, {0., 0., -2.}};
  std::vector<procam::Vec2> x(3);
  for(int i = 0; i < 2; i++)
    m.project(X[i], x[i]);
  CHECK(procam::rms(m, X, x, {0, 1}) == Approx(0.).margin(1e-9));
  CHECK(std::isinf(procam::rms(m, X, x, {0, 1, 2})));
}

TEST_CASE("pose with fixed distortion starts from undistorted points", "[procam][hardening]")
{
  namespace procam = score::gfx::procam;
  procam::Model known;
  known.fx = known.fy = 500.;
  known.cx = 640.;
  known.cy = 400.;
  known.k1 = -0.25;
  known.k2 = 0.05;
  procam::Model truth = known;
  truth.R = Eigen::AngleAxisd(0.3, procam::Vec3(0.2, 1., 0.1).normalized()).toRotationMatrix();
  truth.t = procam::Vec3(0.2, -0.1, 3.);

  std::mt19937 rng{5};
  std::uniform_real_distribution<double> u(-1.5, 1.5);
  std::vector<procam::Vec3> X;
  std::vector<procam::Vec2> x;
  while(X.size() < 40)
  {
    const procam::Vec3 P(u(rng), u(rng), u(rng) * 0.5);
    procam::Vec2 p;
    if(!truth.project(P, p))
      continue;
    X.push_back(P);
    x.push_back(p);
  }
  for(std::size_t i = 0; i < x.size(); i++)
  {
    const procam::Vec3 c = truth.R * X[i] + truth.t;
    const procam::Vec2 n = known.normalise(x[i]);
    CHECK(n.x() == Approx(c.x() / c.z()).margin(1e-9));
    CHECK(n.y() == Approx(c.y() / c.z()).margin(1e-9));
  }

  procam::Options opt;
  opt.maxIterations = 0;
  const auto r = procam::solvePose(X, x, known, opt);
  REQUIRE(r.ok);
  CHECK(r.rms < 1e-6);
  CHECK((r.model.t - truth.t).norm() < 1e-6);

  const auto e = procam::solvePoseAny(X, x, known, opt);
  REQUIRE(e.ok);
  CHECK(e.rms < 1e-6);
}

TEST_CASE("registration RANSAC stops once confident", "[registration][hardening]")
{
  namespace reg = score::gfx::registration;
  std::mt19937 rng{9};
  std::uniform_real_distribution<double> u(-1., 1.);
  std::vector<Eigen::Vector3d> a, b;
  const Eigen::Matrix3d R = Eigen::AngleAxisd(0.4, Eigen::Vector3d::UnitY()).toRotationMatrix();
  for(int i = 0; i < 20000; i++)
  {
    const Eigen::Vector3d p(u(rng), u(rng), u(rng));
    a.push_back(p);
    b.push_back(R * p + Eigen::Vector3d(0.5, 0., 0.));
  }
  const auto r = reg::ransac(a, b, 1e-3, false);
  REQUIRE(r.ok);
  CHECK(r.inliers == 20000);
  CHECK(r.iterations < 20);
}

TEST_CASE("isotonic regression pools the violators", "[photometric]")
{
  const auto a = ph::isotonic({1., 3., 2., 4.}, {1., 1., 1., 1.});
  CHECK(a == std::vector<double>{1., 2.5, 2.5, 4.});
  const auto b = ph::isotonic({5., 1., 2.}, {1., 2., 1.});
  REQUIRE(b.size() == 3);
  for(double v : b)
    CHECK(v == Approx(9. / 4.));
  const auto c = ph::isotonic({2., 1.}, {0., 1.});
  REQUIRE(c.size() == 2);
  CHECK(std::isfinite(c[0]));
  CHECK(c[0] <= c[1]);
  CHECK(ph::isotonic({1., 2.}, {1.}).size() == 2);
  CHECK(ph::psnr({0.}, {0., 1.}) == 0.);
}
