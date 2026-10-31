#pragma once
#include <Eigen/Dense>

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

//! y_i ~ s R x_i + t over the indices given (all when empty).
inline bool umeyama(
    const std::vector<Vec3>& x, const std::vector<Vec3>& y, Transform& out, bool withScale,
    const std::vector<int>& idx = {})
{
  const int n = idx.empty() ? int(x.size()) : int(idx.size());
  if(n < 3 || x.size() != y.size())
    return false;
  auto at = [&](int k) { return idx.empty() ? k : idx[k]; };
  Vec3 mx = Vec3::Zero(), my = Vec3::Zero();
  for(int k = 0; k < n; k++)
  {
    mx += x[at(k)];
    my += y[at(k)];
  }
  mx /= n;
  my /= n;
  Eigen::Matrix3d S = Eigen::Matrix3d::Zero();
  double varx = 0.;
  for(int k = 0; k < n; k++)
  {
    const Vec3 dx = x[at(k)] - mx, dy = y[at(k)] - my;
    S += dy * dx.transpose();
    varx += dx.squaredNorm();
  }
  S /= n;
  varx /= n;
  if(!(varx > 1e-18))
    return false;
  Eigen::JacobiSVD<Eigen::Matrix3d> svd(S, Eigen::ComputeFullU | Eigen::ComputeFullV);
  const auto& D = svd.singularValues();
  if(D(1) < 1e-12 * std::max(1e-300, D(0)))
    return false;
  Eigen::Matrix3d E = Eigen::Matrix3d::Identity();
  if((svd.matrixU() * svd.matrixV().transpose()).determinant() < 0.)
    E(2, 2) = -1.;
  out.R = svd.matrixU() * E * svd.matrixV().transpose();
  out.scale = withScale ? (D.asDiagonal() * E).trace() / varx : 1.;
  out.t = my - out.scale * out.R * mx;
  return out.R.allFinite() && out.t.allFinite();
}

struct Result
{
  bool ok{};
  Transform transform;
  int inliers{};
  double rms{};
  std::string error;
};

inline Result ransac(
    const std::vector<Vec3>& x, const std::vector<Vec3>& y, double threshold, bool withScale,
    int iterations = 2000, uint32_t seed = 0x5eed)
{
  Result r;
  const int n = int(x.size());
  if(n < 3 || x.size() != y.size())
  {
    r.error = "fewer than 3 correspondences";
    return r;
  }
  std::mt19937 rng{seed};
  std::uniform_int_distribution<int> pick(0, n - 1);
  const double th2 = threshold * threshold;
  std::vector<int> best;
  double need = iterations;
  for(int it = 0; it < iterations && it < need; it++)
  {
    int a = pick(rng), b = pick(rng), c = pick(rng);
    if(a == b || b == c || a == c)
      continue;
    const Vec3 u = x[b] - x[a], v = x[c] - x[a];
    if(u.cross(v).norm() < 1e-9 * (u.squaredNorm() + v.squaredNorm()))
      continue;
    Transform T;
    if(!umeyama(x, y, T, withScale, {a, b, c}))
      continue;
    std::vector<int> in;
    for(int k = 0; k < n; k++)
      if((T(x[k]) - y[k]).squaredNorm() <= th2)
        in.push_back(k);
    if(in.size() > best.size())
    {
      best = std::move(in);
      const double w = double(best.size()) / n;
      need = it + 1 + std::log(1. - 0.999) / std::log(std::max(1e-12, 1. - w * w * w));
    }
  }
  if(best.size() < 3)
  {
    r.error = "no consensus";
    return r;
  }
  for(int pass = 0; pass < 3; pass++)
  {
    if(!umeyama(x, y, r.transform, withScale, best))
    {
      r.error = "degenerate correspondences";
      return r;
    }
    std::vector<int> in;
    for(int k = 0; k < n; k++)
      if((r.transform(x[k]) - y[k]).squaredNorm() <= th2)
        in.push_back(k);
    if(in.size() < 3)
      break;
    best = std::move(in);
  }
  double se = 0.;
  for(int k : best)
    se += (r.transform(x[k]) - y[k]).squaredNorm();
  r.inliers = int(best.size());
  r.rms = std::sqrt(se / std::max<std::size_t>(1, best.size()));
  r.ok = true;
  return r;
}
}
