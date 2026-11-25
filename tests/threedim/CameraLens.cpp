#include <Threedim/CameraLens.hpp>
#include <Threedim/LensConversion.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <Eigen/Dense>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;
using namespace Threedim;

TEST_CASE("Camera lens: unproject and project are inverse", "[threedim][lens]")
{
  const std::array<double, 4> K{700., 690., 640., 360.};
  for(auto model : {CameraLens::Pinhole, CameraLens::Distorted, CameraLens::Fisheye})
  {
    CameraLens lens;
    lens.model = model;
    if(model == CameraLens::Distorted)
      lens.k = {-0.2, 0.05, 0.001, -0.0005, 0.};
    if(model == CameraLens::Fisheye)
      lens.k = {0.05, -0.01, 0., 0., 0.};
    INFO(CameraLens::name(model));
    for(double px : {10., 300., 640., 1100., 1270.})
      for(double py : {5., 200., 360., 700.})
      {
        const auto d = lens.unproject(K, px, py);
        CHECK(std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) == Approx(1.).margin(1e-9));
        double x, y;
        REQUIRE(lens.project(K, d, x, y));
        CHECK(x == Approx(px).margin(1e-6));
        CHECK(y == Approx(py).margin(1e-6));
      }
  }
}

TEST_CASE("Camera lens: a 180 degree fisheye sees sideways at its edge", "[threedim][lens]")
{
  const auto K = CameraLens::intrinsicsFromFov(CameraLens::Fisheye, 180., 1001, 1001);
  CameraLens lens;
  lens.model = CameraLens::Fisheye;
  const auto edge = lens.unproject(K, 1000.5, 500.);
  CHECK(edge[0] == Approx(1.).margin(1e-3));
  CHECK(std::abs(edge[2]) < 1e-3);
  const auto centre = lens.unproject(K, 500., 500.);
  CHECK(centre[2] == Approx(1.).margin(1e-9));
  const auto pin = CameraLens::intrinsicsFromFov(CameraLens::Pinhole, 90., 1001, 1001);
  CHECK(pin[0] == Approx(500.5).margin(1e-9));
}

namespace
{
constexpr int W = 200, H = 150;
constexpr int CW = 1280, CH = 720;
const std::array<float, 16> flipM{1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};

//! True world directions of a projector on a wall and the floor, then what a
//! camera with `truth` reports when read as `assumed`: the direction of the
//! pixel the true direction falls on, through the assumed lens.
std::vector<float> measuredWith(const CameraLens& truth, const CameraLens& assumed,
                                const std::array<double, 4>& K)
{
  const Eigen::Vector3d C(0.1, 0.4, -0.2);
  const Eigen::Vector3d fwd = Eigen::Vector3d(0., -0.5, -1.).normalized();
  const Eigen::Vector3d right = fwd.cross(Eigen::Vector3d::UnitY()).normalized();
  const Eigen::Vector3d up = right.cross(fwd);
  const double f = 160.;
  std::vector<float> m(std::size_t(W) * H * 4, 0.f);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const Eigen::Vector3d r
          = (fwd + right * ((x + 0.5 - W / 2.) / f) - up * ((y + 0.5 - H / 2.) / f)).normalized();
      double t = 1e9;
      if(r.z() < 0.)
        t = (-2. - C.z()) / r.z();
      if(r.y() < 0.)
        t = std::min(t, (-1.2 - C.y()) / r.y());
      const Eigen::Vector3d w = (C + t * r).normalized();
      const std::array<double, 3> cam{w.x(), -w.y(), -w.z()};
      double px, py;
      if(!truth.project(K, cam, px, py) || px < 0 || py < 0 || px > CW || py > CH)
        continue;
      const auto d = assumed.unproject(K, px, py);
      float* q = &m[(std::size_t(y) * W + x) * 4];
      q[0] = float(d[0]);
      q[1] = float(-d[1]);
      q[2] = float(-d[2]);
      q[3] = 1.f;
    }
  return m;
}
}

TEST_CASE("Lens conversion re-expresses a map through another lens", "[threedim][lens]")
{
  const auto K = CameraLens::intrinsicsFromFov(CameraLens::Pinhole, 82., CW, CH);
  CameraLens truth;
  truth.model = CameraLens::Distorted;
  truth.k = {-0.15, 0., 0., 0., 0.};
  const CameraLens pinhole{};
  const auto asPinhole = measuredWith(truth, pinhole, K);
  const auto exact = measuredWith(truth, truth, K);
  MeasuredCamera from{K, pinhole, CW, CH}, to{K, truth, CW, CH};
  const auto conv = convertLens(asPinhole, from, to, flipM);
  double worst = 0.;
  for(std::size_t i = 0; i < conv.size(); i += 4)
    if(exact[i + 3] > 0.5f && conv[i + 3] > 0.5f)
    {
      const Eigen::Vector3d u(conv[i], conv[i + 1], conv[i + 2]), v(exact[i], exact[i + 1], exact[i + 2]);
      const double a = std::atan2(u.cross(v).norm(), u.dot(v)) * 180. / M_PI * 60.;
      worst = std::max(worst, a);
    }
  CHECK(worst < 0.1);
}

TEST_CASE("Radial distortion is found from flat surfaces", "[threedim][lens]")
{
  const auto K = CameraLens::intrinsicsFromFov(CameraLens::Pinhole, 82., CW, CH);
  CameraLens truth;
  truth.model = CameraLens::Distorted;
  truth.k = {-0.12, 0., 0., 0., 0.};
  const CameraLens pinhole{};
  const auto m = measuredWith(truth, pinhole, K);
  const auto e = estimateRadialDistortion(
      {m}, {{W, H}}, MeasuredCamera{K, pinhole, CW, CH}, CameraLens::Distorted, K, flipM);
  REQUIRE(e.ok);
  INFO("k1 " << e.k1 << " residual " << e.residual << " at 0 " << e.residualAtZero);
  CHECK(e.k1 == Approx(-0.12).margin(0.02));
  CHECK(e.residual < e.residualAtZero);
}

TEST_CASE("Fisheye radial distortion is found from several maps", "[threedim][lens]")
{
  const auto K = CameraLens::intrinsicsFromFov(CameraLens::Fisheye, 120., CW, CH);
  CameraLens truth;
  truth.model = CameraLens::Fisheye;
  truth.k = {0.08, 0., 0., 0., 0.};
  CameraLens assumed;
  assumed.model = CameraLens::Fisheye;
  const auto m = measuredWith(truth, assumed, K);
  const auto e = estimateRadialDistortion(
      {m, m}, {{W, H}, {W, H}}, MeasuredCamera{K, assumed, CW, CH}, CameraLens::Fisheye, K,
      flipM);
  REQUIRE(e.ok);
  INFO("k1 " << e.k1 << " residual " << e.residual << " at 0 " << e.residualAtZero);
  CHECK(e.k1 == Approx(0.08).margin(0.03));
  CHECK(e.residual < e.residualAtZero);
}

TEST_CASE("A measured camera round-trips and a broken one is refused", "[threedim][lens]")
{
  QTemporaryDir dir;
  MeasuredCamera c;
  c.intrinsics = {800., 810., 640., 360.};
  c.lens.model = CameraLens::Distorted;
  c.lens.k = {-0.1, 0.02, 0., 0., 0.};
  c.width = 1280;
  c.height = 720;
  const QString base = dir.filePath("p");
  REQUIRE(saveMeasuredCamera(base, c));
  const auto back = loadMeasuredCamera(base);
  REQUIRE(back);
  CHECK(back->intrinsics == c.intrinsics);
  CHECK(back->lens == c.lens);
  CHECK(back->width == 1280);

  auto zero = c;
  zero.width = 0;
  REQUIRE(saveMeasuredCamera(base, zero));
  CHECK_FALSE(loadMeasuredCamera(base));
  QFile f{base + ".measured.json"};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write("{ not json");
  f.close();
  CHECK_FALSE(loadMeasuredCamera(base));
}
