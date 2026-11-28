#pragma once
#include <score_plugin_gfx_export.h>
#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// Rigid (or similarity) transform between two sets of corresponding 3D points:
// the least-squares solution of Umeyama 1991 (doi:10.1109/34.88573), the SVD
// form of Horn 1987's absolute orientation (doi:10.1364/JOSAA.4.000629),
// inside RANSAC (Fischler & Bolles 1981, doi:10.1145/358669.358692).
namespace score::gfx::registration
{
using Vec3 = Eigen::Vector3d;

struct Transform
{
  Eigen::Matrix3d R{Eigen::Matrix3d::Identity()};
  Vec3 t{Vec3::Zero()};
  double scale{1.};

  Vec3 operator()(const Vec3& x) const noexcept { return scale * (R * x) + t; }
  Eigen::Matrix4d matrix() const noexcept
  {
    Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
    m.topLeftCorner<3, 3>() = scale * R;
    m.topRightCorner<3, 1>() = t;
    return m;
  }
};

//! y_i ~ s R x_i + t over the indices given (all when empty), by
//! Eigen::umeyama; false for fewer than 3 points or a rank below 2.
SCORE_PLUGIN_GFX_EXPORT bool umeyama(
    const std::vector<Vec3>& x, const std::vector<Vec3>& y, Transform& out, bool withScale,
    const std::vector<int>& idx = {});

struct Result
{
  bool ok{};
  Transform transform;
  int inliers{};
  double rms{};
  int iterations{};
  std::string error;
};

SCORE_PLUGIN_GFX_EXPORT Result ransac(
    const std::vector<Vec3>& x, const std::vector<Vec3>& y, double threshold, bool withScale,
    int iterations = 2000, uint32_t seed = 0x5eed);
}
