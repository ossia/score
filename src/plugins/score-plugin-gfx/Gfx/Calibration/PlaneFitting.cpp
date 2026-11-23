#include <Gfx/Calibration/PlaneFitting.hpp>
#include <Eigen/Dense>

namespace score::gfx::planes
{

bool fitPlane(
    const std::vector<Eigen::Vector3d>& pts, const std::vector<int>& idx, Plane& out)
{
  if(idx.size() < 3)
    return false;
  Eigen::Vector3d c = Eigen::Vector3d::Zero();
  for(int i : idx)
    c += pts[i];
  c /= double(idx.size());
  Eigen::Matrix3d cov = Eigen::Matrix3d::Zero();
  for(int i : idx)
  {
    const Eigen::Vector3d q = pts[i] - c;
    cov += q * q.transpose();
  }
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(cov);
  out.n = es.eigenvectors().col(0).normalized();
  out.d = -out.n.dot(c);
  return out.n.allFinite();
}

std::vector<Plane> fitPlanes(
    const std::vector<Eigen::Vector3d>& pts, const Eigen::Vector3d& viewpoint,
    const Options& opt)
{
  std::vector<Plane> planes;
  std::vector<int> remaining(pts.size());
  for(std::size_t i = 0; i < pts.size(); i++)
    remaining[i] = int(i);

  std::mt19937 rng(opt.seed);
  for(int p = 0; p < opt.maxPlanes; p++)
  {
    const int n = int(remaining.size());
    if(n < std::max(3, opt.minInliers))
      break;
    std::uniform_int_distribution<int> pick(0, n - 1);

    int bestCount = 0;
    Plane best;
    for(int it = 0; it < opt.iterations; it++)
    {
      const auto& a = pts[remaining[pick(rng)]];
      const auto& b = pts[remaining[pick(rng)]];
      const auto& c = pts[remaining[pick(rng)]];
      Eigen::Vector3d nrm = (b - a).cross(c - a);
      const double len = nrm.norm();
      if(!(len > 1e-12))
        continue;
      nrm /= len;
      const double d = -nrm.dot(a);
      int count = 0;
      for(int i : remaining)
        if(std::abs(nrm.dot(pts[i]) + d) < opt.threshold)
          count++;
      if(count > bestCount)
      {
        bestCount = count;
        best.n = nrm;
        best.d = d;
      }
    }
    if(bestCount < std::max(3, opt.minInliers))
      break;

    // Points within the threshold of two planes (along their intersection)
    // tilt a least-squares refit, so it uses a tighter band, re-centred on
    // each refit.
    const double band = opt.threshold / 3.;
    for(int pass = 0; pass < 3; pass++)
    {
      std::vector<int> core;
      for(int i : remaining)
        if(std::abs(best.n.dot(pts[i]) + best.d) < band)
          core.push_back(i);
      Plane refined;
      if(core.size() < 3 || !fitPlane(pts, core, refined))
        break;
      best.n = refined.n;
      best.d = refined.d;
    }
    std::vector<int> in, out;
    for(int i : remaining)
      (std::abs(best.n.dot(pts[i]) + best.d) < opt.threshold ? in : out).push_back(i);
    best.inliers = int(in.size());
    if(best.n.dot(viewpoint) + best.d < 0.)
    {
      best.n = -best.n;
      best.d = -best.d;
    }
    planes.push_back(best);
    remaining = std::move(out);
  }
  return planes;
}
}
