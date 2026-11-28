#pragma once
#include <score_plugin_gfx_export.h>

#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// Pinhole projector / camera model and its calibration from 3D <-> 2D
// correspondences: OpenCV local frame (+X right, +Y down, +Z forward), pixel
// centres at integer coordinates, radial distortion k1 k2 in OpenCV's model.
// Model::R and t are localFromWorld (x_local = R X + t); worldFromLocal()
// and position() give the projector in the world.
namespace score::gfx::procam
{
using Vec2 = Eigen::Vector2d;
using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;
using Mat34 = Eigen::Matrix<double, 3, 4>;

struct Model
{
  double fx{1.}, fy{1.}, cx{0.}, cy{0.};
  double k1{}, k2{};
  //! localFromWorld
  Mat3 R{Mat3::Identity()};
  Vec3 t{Vec3::Zero()};

  //! false when the point is behind the projector.
  bool project(const Vec3& X, Vec2& px) const noexcept
  {
    const Vec3 c = R * X + t;
    if(!(c.z() > 1e-12))
      return false;
    const double xn = c.x() / c.z(), yn = c.y() / c.z();
    const double r2 = xn * xn + yn * yn;
    const double d = 1. + k1 * r2 + k2 * r2 * r2;
    px = {fx * xn * d + cx, fy * yn * d + cy};
    return true;
  }

  Eigen::Matrix4d worldFromLocal() const noexcept
  {
    Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
    m.topLeftCorner<3, 3>() = R.transpose();
    m.topRightCorner<3, 1>() = -R.transpose() * t;
    return m;
  }

  Vec3 position() const noexcept { return -R.transpose() * t; }

  //! Pixel -> undistorted normalised image coordinates (fixed point on k1 k2).
  Vec2 normalise(const Vec2& px) const noexcept
  {
    const Vec2 xd((px.x() - cx) / fx, (px.y() - cy) / fy);
    if(k1 == 0. && k2 == 0.)
      return xd;
    Vec2 xn = xd;
    for(int it = 0; it < 20; it++)
    {
      const double r2 = xn.squaredNorm();
      const double d = 1. + k1 * r2 + k2 * r2 * r2;
      if(!(std::abs(d) > 1e-9))
        break;
      const Vec2 next = xd / d;
      if(!next.allFinite())
        break;
      const bool done = (next - xn).squaredNorm() < 1e-24;
      xn = next;
      if(done)
        break;
    }
    return xn;
  }
};

struct Options
{
  bool distortion{false};
  bool ransac{false};
  double ransacThreshold{3.};
  int ransacIterations{1000};
  double ransacConfidence{0.995};
  int maxIterations{200};
  uint32_t seed{0x5eed};
};

struct Result
{
  bool ok{};
  Model model;
  double rms{};
  int inliers{};
  std::vector<int> mask;
  std::string error;
};

namespace detail
{
// Hartley normalisation: centroid at the origin, mean distance sqrt(dim).
template <int D>
Eigen::Matrix<double, D + 1, D + 1> normalisation(
    const std::vector<Eigen::Matrix<double, D, 1>>& pts, const std::vector<int>& idx)
{
  Eigen::Matrix<double, D, 1> c = Eigen::Matrix<double, D, 1>::Zero();
  for(int i : idx)
    c += pts[i];
  c /= double(idx.size());
  double mean = 0.;
  for(int i : idx)
    mean += (pts[i] - c).norm();
  mean /= double(idx.size());
  const double s = mean > 1e-12 ? std::sqrt(double(D)) / mean : 1.;
  Eigen::Matrix<double, D + 1, D + 1> T = Eigen::Matrix<double, D + 1, D + 1>::Identity();
  T.template topLeftCorner<D, D>() *= s;
  T.template topRightCorner<D, 1>() = -s * c;
  return T;
}
}

//! Direct linear transform for the 3x4 projection over the selected points.
SCORE_PLUGIN_GFX_EXPORT bool dlt(
    const std::vector<Vec3>& X, const std::vector<Vec2>& x, const std::vector<int>& idx,
    Mat34& P);

//! P = K [R | t], K upper triangular with a positive diagonal, skew dropped.
SCORE_PLUGIN_GFX_EXPORT bool decompose(Mat34 P, Model& m);

inline double reprojectionError(const Model& m, const Vec3& X, const Vec2& x) noexcept
{
  Vec2 p;
  if(!m.project(X, p))
    return std::numeric_limits<double>::infinity();
  return (p - x).norm();
}

SCORE_PLUGIN_GFX_EXPORT double rms(
    const Model& m, const std::vector<Vec3>& X, const std::vector<Vec2>& x,
    const std::vector<int>& idx);

//! Levenberg-Marquardt on fx fy cx cy, rotation (incremental), t, and
//! optionally k1 k2, minimising the pixel reprojection error. With poseOnly
//! the intrinsics (and distortion) are kept as they are.
SCORE_PLUGIN_GFX_EXPORT void refine(
    Model& m, const std::vector<Vec3>& X, const std::vector<Vec2>& x,
    const std::vector<int>& idx, bool distortion, int maxIterations,
    bool poseOnly = false);

//! The centroid of the points and their principal axes, by increasing spread.
struct PointSpread
{
  Vec3 centroid = Vec3::Zero();
  Vec3 variance = Vec3::Zero();
  Mat3 axes = Mat3::Identity();
};

SCORE_PLUGIN_GFX_EXPORT PointSpread spread(const std::vector<Vec3>& X, const std::vector<int>& idx);

inline bool coplanar(const std::vector<Vec3>& X, const std::vector<int>& idx)
{
  const auto ev = spread(X, idx).variance;
  return !(ev(2) > 0.) || ev(0) < 1e-8 * ev(2);
}

inline bool collinear(const std::vector<Vec3>& X, const std::vector<int>& idx)
{
  const auto ev = spread(X, idx).variance;
  return !(ev(2) > 0.) || ev(1) < 1e-8 * ev(2);
}

SCORE_PLUGIN_GFX_EXPORT Result solve(
    const std::vector<Vec3>& X, const std::vector<Vec2>& x, const Options& opt = {});

namespace detail
{
//! [R | t] from 6+ non-coplanar points with known intrinsics, on normalised
//! image coordinates.
SCORE_PLUGIN_GFX_EXPORT bool poseDLT(
    const std::vector<Vec3>& X, const std::vector<Vec2>& xn, const std::vector<int>& idx,
    Mat3& R, Vec3& t);

//! [R | t] from 4+ coplanar points with known intrinsics: the homography from
//! the plane to the normalised image is [r1 r2 t] up to scale.
SCORE_PLUGIN_GFX_EXPORT bool posePlanar(
    const std::vector<Vec3>& X, const std::vector<Vec2>& xn, const std::vector<int>& idx,
    Mat3& R, Vec3& t);
}

//! Pose only, the intrinsics (and distortion) of `known` being fixed: RANSAC
//! over minimal samples (6 points, or 4 when the points are coplanar), then
//! Levenberg-Marquardt on R and t.
SCORE_PLUGIN_GFX_EXPORT Result solvePose(
    const std::vector<Vec3>& X, const std::vector<Vec2>& x, const Model& known,
    const Options& opt = {});

//! Column-major 4x4.
inline std::array<float, 16> toColumnMajor(const Eigen::Matrix4d& m) noexcept
{
  std::array<float, 16> out{};
  for(int c = 0; c < 4; c++)
    for(int r = 0; r < 4; r++)
      out[c * 4 + r] = float(m(r, c));
  return out;
}

inline Eigen::Matrix4d fromColumnMajor(const std::array<float, 16>& a) noexcept
{
  Eigen::Matrix4d m;
  for(int c = 0; c < 4; c++)
    for(int r = 0; r < 4; r++)
      m(r, c) = a[c * 4 + r];
  return m;
}

//! World (OpenGL axes: +Y up, -Z forward) from an OpenCV camera frame: (x, -y, -z).
inline constexpr std::array<float, 16> openCvToWorld{
    1.f, 0.f, 0.f, 0.f, 0.f, -1.f, 0.f, 0.f, 0.f, 0.f, -1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
}
