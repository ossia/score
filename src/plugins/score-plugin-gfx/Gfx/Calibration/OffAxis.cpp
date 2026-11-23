#include <Gfx/Calibration/OffAxis.hpp>
#include <Eigen/Dense>

namespace score::gfx::offaxis
{

Frustum kooima(const Vec3& pa, const Vec3& pb, const Vec3& pc, const Vec3& pe)
{
  Frustum f;
  const Vec3 r = pb - pa, u = pc - pa;
  if(r.norm() < 1e-12 || u.norm() < 1e-12 || r.cross(u).norm() < 1e-12)
    return f;
  f.vr = r.normalized();
  f.vn = f.vr.cross(u).normalized();
  f.vu = f.vn.cross(f.vr);
  const Vec3 va = pa - pe, vb = pb - pe, vc = pc - pe;
  f.d = -f.vn.dot(va);
  if(!(f.d > 1e-12))
    return f;
  f.l = f.vr.dot(va) / f.d;
  f.r = f.vr.dot(vb) / f.d;
  f.b = f.vu.dot(va) / f.d;
  f.t = f.vu.dot(vc) / f.d;
  f.ok = true;
  return f;
}

ViewerFrustum fitViewerFrustum(
    const std::vector<float>& xyz, int W, int H, const Vec3& pe, double planeTolerance,
    double oversampling, double percentile, int maxSize)
{
  ViewerFrustum out;
  if(W <= 0 || H <= 0 || xyz.size() < std::size_t(W) * H * 4)
  {
    out.error = "the XYZ map does not match its size";
    return out;
  }
  std::vector<Vec3> pts;
  const int step = std::max(1, int(std::sqrt(double(W) * H / 200000.)));
  for(int v = 0; v < H; v += step)
    for(int u = 0; u < W; u += step)
    {
      const float* p = xyz.data() + (std::size_t(v) * W + u) * 4;
      if(p[3] > 0.f && std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]))
        pts.emplace_back(p[0], p[1], p[2]);
    }
  if(pts.size() < 3)
  {
    out.error = "fewer than 3 lit pixels";
    return out;
  }

  Vec3 c = Vec3::Zero();
  for(auto& p : pts)
    c += p;
  c /= double(pts.size());
  Eigen::Matrix3d C = Eigen::Matrix3d::Zero();
  for(auto& p : pts)
    C += (p - c) * (p - c).transpose();
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(C);
  Vec3 n = es.eigenvectors().col(0);
  double se = 0.;
  for(auto& p : pts)
    se += std::pow(n.dot(p - c), 2.);
  out.planeRms = std::sqrt(se / pts.size());
  out.planar = out.planeRms <= planeTolerance;

  Vec3 axis;
  if(out.planar)
  {
    if(n.dot(pe - c) < 0.)
      n = -n;
    if(std::abs(n.dot(pe - c)) < 1e-9)
    {
      out.error = "the viewer is in the plane of the surface";
      return out;
    }
    axis = -n;
  }
  else
  {
    axis = Vec3::Zero();
    for(auto& p : pts)
      axis += (p - pe).normalized();
    if(axis.norm() < 1e-6)
    {
      out.error = "the surface surrounds the viewer: use a cube map";
      return out;
    }
    axis.normalize();
  }
  Vec3 up(0., 1., 0.);
  if(std::abs(axis.dot(up)) > 0.99)
    up = Vec3(0., 0., axis.y() > 0. ? 1. : -1.);
  Frustum& f = out.frustum;
  f.vn = -axis;
  f.vr = axis.cross(up).normalized();
  f.vu = f.vn.cross(f.vr).normalized();

  double l = 1e300, r = -1e300, b = 1e300, t = -1e300;
  auto tangents = [&](const Vec3& p, double& x, double& y) {
    const Vec3 dd = p - pe;
    const double z = -f.vn.dot(dd);
    if(!(z > 1e-9))
      return false;
    x = f.vr.dot(dd) / z;
    y = f.vu.dot(dd) / z;
    return true;
  };
  for(auto& p : pts)
  {
    double x, y;
    if(!tangents(p, x, y))
    {
      out.error = "part of the surface is behind the viewer: use a cube map";
      return out;
    }
    l = std::min(l, x);
    r = std::max(r, x);
    b = std::min(b, y);
    t = std::max(t, y);
  }
  if(std::max({std::abs(l), std::abs(r), std::abs(b), std::abs(t)}) > 50.)
  {
    out.error = "the surface spans more than ~178 degrees from the viewer: use a cube map";
    return out;
  }
  f.l = l;
  f.r = r;
  f.b = b;
  f.t = t;
  f.d = out.planar ? std::abs(n.dot(pe - c)) : 1.;
  f.ok = true;
  const Vec3 fwd = -f.vn;
  out.pa = pe + f.d * (fwd + l * f.vr + b * f.vu);
  out.pb = pe + f.d * (fwd + r * f.vr + b * f.vu);
  out.pc = pe + f.d * (fwd + l * f.vr + t * f.vu);

  // Pixel footprints in tangent space, along each axis of the projector.
  std::vector<double> dx, dy, ang;
  const double minX = 1e-6 * (r - l), minY = 1e-6 * (t - b);
  const int s2 = std::max(1, step / 2);
  for(int v = 0; v + 1 < H; v += s2)
    for(int u = 0; u + 1 < W; u += s2)
    {
      const std::size_t i = std::size_t(v) * W + u;
      const float* p = xyz.data() + i * 4;
      const float* pu = p + 4;
      const float* pv = xyz.data() + (i + W) * 4;
      if(p[3] <= 0.f || pu[3] <= 0.f || pv[3] <= 0.f)
        continue;
      double x0, y0, x1, y1, x2, y2;
      const Vec3 P(p[0], p[1], p[2]), Pu(pu[0], pu[1], pu[2]), Pv(pv[0], pv[1], pv[2]);
      if(!tangents(P, x0, y0) || !tangents(Pu, x1, y1) || !tangents(Pv, x2, y2))
        continue;
      const double fx = std::max(std::abs(x1 - x0), std::abs(x2 - x0));
      const double fy = std::max(std::abs(y1 - y0), std::abs(y2 - y0));
      const double z2 = 1. + x0 * x0 + y0 * y0;
      if(fx > minX)
        dx.push_back(fx);
      if(fy > minY)
        dy.push_back(fy);
      if(fx > minX || fy > minY)
        ang.push_back(std::sqrt(fx * fx + fy * fy) / std::sqrt(2.) / z2);
    }
  if(dx.empty() || dy.empty())
  {
    out.error = "no pair of distinct neighbouring lit pixels";
    return out;
  }
  auto quantile = [](std::vector<double> v, double q) {
    q = q >= 0. ? std::min(q, 1.) : 0.;
    const std::size_t k = std::min(v.size() - 1, std::size_t(q * (v.size() - 1)));
    std::nth_element(v.begin(), v.begin() + k, v.end());
    return v[k];
  };
  const double px = std::max(1e-9, quantile(dx, percentile));
  const double py = std::max(1e-9, quantile(dy, percentile));
  out.pixelMedian = quantile(ang, 0.5);
  out.pixelPercentile = quantile(ang, percentile);
  const double os = oversampling > 0. ? oversampling : 1.;
  auto size = [&](double v) {
    return int(std::clamp(std::ceil(v), 1., double(std::max(1, maxSize))));
  };
  out.width = size(os * (r - l) / px);
  out.height = size(os * (t - b) / py);
  out.ok = true;
  return out;
}
}
