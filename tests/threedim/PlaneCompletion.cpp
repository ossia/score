#include <Threedim/PlaneCompletion.hpp>

#include <Eigen/Dense>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>

using namespace Threedim;

constexpr int W = 160, H = 120;
namespace
{

//! Directions from the viewer at the origin to where a projector above it
//! lands on the front wall (z = -2) and the floor (y = -1.2).
std::vector<float> truth(bool floorToo, int W = ::W, int H = ::H)
{
  const Eigen::Vector3d C(0.1, 0.4, -0.2);
  const Eigen::Vector3d fwd = Eigen::Vector3d(0., floorToo ? -0.7 : -0.1, -1.).normalized();
  const Eigen::Vector3d right = fwd.cross(Eigen::Vector3d::UnitY()).normalized();
  const Eigen::Vector3d up = right.cross(fwd);
  const double f = 140. * W / 160.;
  std::vector<float> d(std::size_t(W) * H * 4, 0.f);
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
      const Eigen::Vector3d p = (C + t * r).normalized();
      float* q = &d[(std::size_t(y) * W + x) * 4];
      q[0] = float(p.x());
      q[1] = float(p.y());
      q[2] = float(p.z());
      q[3] = 1.f;
    }
  return d;
}

std::vector<float> measure(const std::vector<float>& t, int W = ::W, int H = ::H)
{
  std::mt19937 rng{7};
  std::normal_distribution<double> noise(0., 1. / 60. * M_PI / 180.);
  std::uniform_real_distribution<double> u(0., 1.);
  auto m = t;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      float* q = &m[(std::size_t(y) * W + x) * 4];
      const bool hole = (x > 60 * W / 160 && x < 110 * W / 160 && y > 20 * H / 120 && y < 50 * H / 120)
                        || u(rng) < 0.2;
      if(hole)
      {
        q[3] = 0.f;
        continue;
      }
      if(u(rng) < 0.02)
      {
        q[0] += 0.2f;
        continue;
      }
      for(int c = 0; c < 3; c++)
        q[c] += float(noise(rng));
    }
  return m;
}

double arcmin(const float* a, const float* b)
{
  const double dot = (double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2])
                     / std::sqrt((double(a[0]) * a[0] + double(a[1]) * a[1] + double(a[2]) * a[2])
                                 * (double(b[0]) * b[0] + double(b[1]) * b[1] + double(b[2]) * b[2]));
  return std::acos(std::clamp(dot, -1., 1.)) * 180. / M_PI * 60.;
}
}

TEST_CASE("Plane completion rebuilds a wall-and-floor projector from its planes", "[threedim][calibration][planes]")
{
  const auto t = truth(true);
  const auto r = completeFromPlanes(measure(t), W, H);
  INFO(r.status);
  REQUIRE(r.ok);
  CHECK(r.homographies.size() == 2);
  for(double m : r.medianArcmin)
    CHECK(m < 3.);
  CHECK(r.explained > 0.9);
  std::vector<double> err;
  for(std::size_t i = 0; i < std::size_t(W) * H; i++)
  {
    CHECK(r.directions[i * 4 + 3] == 1.f);
    err.push_back(arcmin(&r.directions[i * 4], &t[i * 4]));
  }
  std::sort(err.begin(), err.end());
  INFO("median " << err[err.size() / 2] << " p99 " << err[err.size() * 99 / 100] << " max "
                 << err.back());
  CHECK(err[err.size() / 2] < 1.);
  CHECK(err[err.size() * 99 / 100] < 3.);
  CHECK(err.back() < 10.);
}

TEST_CASE("Plane completion of a projector on one wall uses one plane", "[threedim][calibration][planes]")
{
  const auto t = truth(false);
  const auto r = completeFromPlanes(measure(t), W, H);
  REQUIRE(r.ok);
  CHECK(r.homographies.size() == 1);
  double worst = 0.;
  for(std::size_t i = 0; i < std::size_t(W) * H; i++)
    worst = std::max(worst, arcmin(&r.directions[i * 4], &t[i * 4]));
  CHECK(worst < 3.);
}

TEST_CASE("Plane completion refuses a map with nothing measured", "[threedim][calibration][planes]")
{
  std::vector<float> d(std::size_t(W) * H * 4, 0.f);
  const auto r = completeFromPlanes(d, W, H);
  CHECK_FALSE(r.ok);
}

namespace
{
//! A projector off-centre in a dome of radius 1.5 around the viewer.
std::vector<float> dome()
{
  const Eigen::Vector3d C(0.6, 0.5, 0.3);
  const Eigen::Vector3d fwd = Eigen::Vector3d(-0.3, 0.2, -1.).normalized();
  const Eigen::Vector3d right = fwd.cross(Eigen::Vector3d::UnitY()).normalized();
  const Eigen::Vector3d up = right.cross(fwd);
  std::vector<float> d(std::size_t(W) * H * 4, 0.f);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const Eigen::Vector3d r
          = (fwd + right * ((x + 0.5 - W / 2.) / 90.) - up * ((y + 0.5 - H / 2.) / 90.)).normalized();
      const double b = C.dot(r), c = C.squaredNorm() - 1.5 * 1.5;
      const Eigen::Vector3d p = (C + (-b + std::sqrt(b * b - c)) * r).normalized();
      float* q = &d[(std::size_t(y) * W + x) * 4];
      q[0] = float(p.x());
      q[1] = float(p.y());
      q[2] = float(p.z());
      q[3] = 1.f;
    }
  return d;
}
}

TEST_CASE("Plane completion keeps the measurement of a curved screen", "[threedim][calibration][planes]")
{
  const auto d = dome();
  const auto r = completeFromPlanes(d, W, H);
  INFO(r.status << " explained " << r.explained);
  CHECK_FALSE(r.ok);
  CHECK(r.explained < 0.8);
  CHECK(r.status.find("not flat") != std::string::npos);
  CHECK(planeResidual(d, W, H) > 4. * planeResidual(measure(truth(true)), W, H));
}

TEST_CASE("Plane completion takes at most two planes", "[threedim][calibration][planes]")
{
  const auto m = measure(truth(true));
  const auto many = completeFromPlanes(m, W, H, 6., 5);
  INFO(many.status);
  REQUIRE(many.ok);
  CHECK(many.homographies.size() == 2);
  const auto none = completeFromPlanes(m, W, H, 6., 0);
  INFO(none.status);
  CHECK(none.homographies.size() == 1);
  CHECK_FALSE(none.ok);
  CHECK(planeResidual(std::vector<float>(std::size_t(W) * H * 4, 0.f), W, H) < 0.);
}

TEST_CASE("Plane completion at 3840x2160", "[.][bench][threedim][planes]")
{
  constexpr int BW = 3840, BH = 2160;
  const auto t = truth(true, BW, BH);
  const auto m = measure(t, BW, BH);
  const auto t0 = std::chrono::steady_clock::now();
  const auto r = completeFromPlanes(m, BW, BH);
  const double s
      = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  WARN("3840x2160: " << s << " s, " << r.homographies.size() << " planes");
  CHECK(r.homographies.size() == 2);
}
