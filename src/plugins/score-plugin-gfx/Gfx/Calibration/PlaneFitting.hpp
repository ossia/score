#pragma once
#include <score_plugin_gfx_export.h>

#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace score::gfx::planes
{
//! n . p + d = 0, n unit.
struct Plane
{
  Eigen::Vector3d n{0., 1., 0.};
  double d{};
  int inliers{};
};

struct Options
{
  int maxPlanes{4};
  double threshold{0.02};
  int iterations{500};
  int minInliers{100};
  uint32_t seed{0x9a7e};
};

//! Least-squares plane through the selected points.
SCORE_PLUGIN_GFX_EXPORT bool fitPlane(
    const std::vector<Eigen::Vector3d>& pts, const std::vector<int>& idx, Plane& out);

//! Sequential RANSAC: the largest plane, its inliers removed, then the next.
//! Every normal is turned toward `viewpoint` (n . viewpoint + d > 0).
SCORE_PLUGIN_GFX_EXPORT std::vector<Plane> fitPlanes(
    const std::vector<Eigen::Vector3d>& pts, const Eigen::Vector3d& viewpoint,
    const Options& opt);
}
