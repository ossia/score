#include <Threedim/PoseRefinement.hpp>

#include <Eigen/Dense>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace Threedim;

namespace
{
constexpr int W = 160, H = 100;

Eigen::Matrix3d euler(const std::array<float, 3>& r)
{
  return (Eigen::AngleAxisd(r[1] * M_PI / 180., Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(r[0] * M_PI / 180., Eigen::Vector3d::UnitX())
          * Eigen::AngleAxisd(r[2] * M_PI / 180., Eigen::Vector3d::UnitZ()))
      .toRotationMatrix();
}

//! What the camera at (camPos, camRot) measures for a projector at C looking
//! along fwd: per projector pixel, the camera-frame direction to its hit.
std::vector<float> measure(
    const RoomGeometry& room, const Eigen::Vector3d& C, Eigen::Vector3d fwd,
    const std::array<float, 3>& camPos, const std::array<float, 3>& camRot)
{
  fwd.normalize();
  const Eigen::Vector3d right = fwd.cross(Eigen::Vector3d::UnitY()).normalized();
  const Eigen::Vector3d up = right.cross(fwd);
  const Eigen::Matrix3d R = euler(camRot);
  const Eigen::Vector3d O(camPos[0], camPos[1], camPos[2]);
  std::vector<float> m(std::size_t(W) * H * 4, 0.f);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const Eigen::Vector3d r
          = (fwd + right * ((x + 0.5 - W / 2.) / 130.) - up * ((y + 0.5 - H / 2.) / 130.))
                .normalized();
      const float t = room.cast(
          {float(C.x()), float(C.y()), float(C.z())},
          {float(r.x()), float(r.y()), float(r.z())});
      if(t <= 0.f)
        continue;
      const Eigen::Vector3d X = C + double(t) * r;
      const Eigen::Vector3d d = (R.transpose() * (X - O)).normalized();
      float* q = &m[(std::size_t(y) * W + x) * 4];
      q[0] = float(d.x());
      q[1] = float(d.y());
      q[2] = float(d.z());
      q[3] = 1.f;
    }
  return m;
}
}

TEST_CASE("The camera pose is found from the outputs' projector consistency", "[threedim][pose]")
{
  const auto room = RoomGeometry::box(3.f, 2.f, 2.5f, 2.5f, 1.4f, 1.6f);
  const std::array<float, 3> truePos{0.2f, 0.1f, -0.3f}, trueRot{3.f, -5.f, 1.f};
  std::vector<std::vector<float>> maps{
      measure(room, {0.0, 1.2, 1.0}, {0.0, -0.7, -1.0}, truePos, trueRot),
      measure(room, {0.5, 1.0, 0.5}, {-1.0, -0.6, -0.3}, truePos, trueRot),
      measure(room, {-0.5, 0.8, 0.0}, {1.0, 0.0, -0.2}, truePos, trueRot)};
  std::vector<DirectionMap> dm;
  for(auto& m : maps)
    dm.push_back({&m, W, H});

  const auto fit = refineCameraPose(dm, room, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f});
  INFO("before " << fit.residualBefore << " after " << fit.residualAfter << " pos "
                 << fit.position[0] << " " << fit.position[1] << " " << fit.position[2]
                 << " rot " << fit.rotation[0] << " " << fit.rotation[1] << " "
                 << fit.rotation[2]);
  REQUIRE(fit.ok);
  CHECK(fit.outputsUsed == 3);
  CHECK(fit.residualAfter < 0.2);
  CHECK(fit.residualBefore > 5. * fit.residualAfter);
  for(int i = 0; i < 3; i++)
  {
    CHECK(std::abs(fit.position[i] - truePos[i]) < 0.03f);
    CHECK(std::abs(fit.rotation[i] - trueRot[i]) < 0.3f);
  }
}

TEST_CASE("Two runs of one output are aligned by the camera's rotation", "[threedim][pose]")
{
  const auto room = RoomGeometry::box(3.f, 2.f, 2.5f, 2.5f, 1.4f, 1.6f);
  const std::array<float, 3> rot{4.f, 9.f, -2.f};
  auto first = measure(room, {0.0, 1.2, 1.0}, {0.0, -0.7, -1.0}, {0, 0, 0}, {0, 0, 0});
  auto turned = measure(room, {0.0, 1.2, 1.0}, {0.0, -0.7, -1.0}, {0, 0, 0}, rot);
  for(std::size_t i = 0; i < turned.size(); i += 4 * 7)
    turned[i + 3] = 0.f;
  const auto a = alignDirections({{&turned, &first}});
  REQUIRE(a.ok);
  CHECK(a.residualArcmin < 0.05);
  CHECK(a.angleDeg > 5.);
  rotateDirections(turned, a.rotation);
  double worst = 0.;
  for(std::size_t i = 0; i < turned.size(); i += 4)
    if(turned[i + 3] > 0.5f && first[i + 3] > 0.5f)
    {
      const Eigen::Vector3d u(turned[i], turned[i + 1], turned[i + 2]),
          v(first[i], first[i + 1], first[i + 2]);
      worst = std::max(worst, std::atan2(u.cross(v).norm(), u.dot(v)) * 180. / M_PI * 60.);
    }
  CHECK(worst < 0.05);
}
