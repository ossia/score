#include <Gfx/Calibration/Registration.hpp>
#include <Eigen/Dense>

namespace score::gfx::registration
{

bool umeyama(
    const std::vector<Vec3>& x, const std::vector<Vec3>& y, Transform& out, bool withScale,
    const std::vector<int>& idx)
{
  const int n = idx.empty() ? int(x.size()) : int(idx.size());
  if(n < 3 || x.size() != y.size())
    return false;
  Eigen::Matrix3Xd src(3, n), dst(3, n);
  for(int k = 0; k < n; k++)
  {
    const int i = idx.empty() ? k : idx[k];
    src.col(k) = x[i];
    dst.col(k) = y[i];
  }
  if(!src.allFinite() || !dst.allFinite())
    return false;
  const Eigen::Matrix3Xd cx = src.colwise() - src.rowwise().mean();
  const Eigen::Matrix3Xd cy = dst.colwise() - dst.rowwise().mean();
  if(!(cx.squaredNorm() / n > 1e-18))
    return false;
  const Eigen::Vector3d D = Eigen::JacobiSVD<Eigen::Matrix3d>(cy * cx.transpose()).singularValues();
  if(D(1) < 1e-12 * std::max(1e-300, D(0)))
    return false;
  const Eigen::Matrix4d T = Eigen::umeyama(src, dst, withScale);
  out.scale = withScale ? T.topLeftCorner<3, 3>().col(0).norm() : 1.;
  out.R = T.topLeftCorner<3, 3>() / out.scale;
  out.t = T.topRightCorner<3, 1>();
  return out.R.allFinite() && out.t.allFinite();
}

Result ransac(
    const std::vector<Vec3>& x, const std::vector<Vec3>& y, double threshold, bool withScale,
    int iterations, uint32_t seed)
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
    r.iterations = it + 1;
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
