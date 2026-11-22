#include "PoseRefinement.hpp"

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Calibration/Registration.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>

namespace Threedim
{
namespace procam = score::gfx::procam;
namespace reg = score::gfx::registration;
namespace
{
struct Corr
{
  Eigen::Vector3d X;
  Eigen::Vector2d p;
};

double fitResidual(const std::vector<Corr>& c)
{
  if(c.size() < 12)
    return -1.;
  std::vector<procam::Vec3> X;
  std::vector<procam::Vec2> x;
  std::vector<int> idx;
  X.reserve(c.size());
  x.reserve(c.size());
  for(const auto& k : c)
  {
    idx.push_back(int(X.size()));
    X.push_back(k.X);
    x.push_back(k.p);
  }
  procam::Mat34 P;
  if(procam::coplanar(X, idx) || !procam::dlt(X, x, idx, P))
    return -1.;
  double e2 = 0.;
  for(std::size_t i = 0; i < X.size(); i++)
  {
    const Eigen::Vector3d q = P * X[i].homogeneous();
    if(!(std::abs(q.z()) > 1e-12))
      return -1.;
    e2 += (q.hnormalized() - x[i]).squaredNorm();
  }
  return std::sqrt(e2 / double(X.size()));
}

//! Whether nearly all the points (90 %, within 1 cm) lie on one plane,
//! which any projector pose explains.
bool planar(const std::vector<Corr>& c)
{
  if(c.size() < 3)
    return true;
  const std::size_t need = c.size() * 9 / 10;
  uint64_t state = 0x5eed;
  auto next = [&] {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return std::size_t(state >> 33) % c.size();
  };
  for(int it = 0; it < 300; it++)
  {
    const auto& a = c[next()].X;
    const auto& b = c[next()].X;
    const auto& d = c[next()].X;
    Eigen::Vector3d n = (b - a).cross(d - a);
    if(!(n.norm() > 1e-9))
      continue;
    n.normalize();
    std::size_t in = 0;
    for(const auto& k : c)
      in += std::abs(n.dot(k.X - a)) < 0.01;
    if(in >= need)
      return true;
  }
  return false;
}

struct Sampled
{
  std::vector<std::array<float, 3>> dirs;
  std::vector<Eigen::Vector2d> pix;
};
}

DirectionAlignment alignDirections(
    const std::vector<std::pair<const std::vector<float>*, const std::vector<float>*>>& maps)
{
  DirectionAlignment a;
  std::vector<reg::Vec3> from, to;
  for(const auto& [f, t] : maps)
  {
    if(!f || !t || f->size() != t->size())
      continue;
    const std::size_t n = f->size() / 4;
    const std::size_t step = std::max<std::size_t>(1, n / 20000);
    for(std::size_t i = 0; i < n; i += step)
    {
      const float* p = &(*f)[i * 4];
      const float* q = &(*t)[i * 4];
      if(!(p[3] > 0.5f) || !(q[3] > 0.5f))
        continue;
      Eigen::Vector3d u(p[0], p[1], p[2]), v(q[0], q[1], q[2]);
      if(!(u.norm() > 1e-9) || !(v.norm() > 1e-9))
        continue;
      from.push_back(u.normalized());
      to.push_back(v.normalized());
    }
  }
  a.pairs = int(from.size());
  if(from.size() < 10)
    return a;
  from.push_back(reg::Vec3::Zero());
  to.push_back(reg::Vec3::Zero());
  const auto res = reg::ransac(from, to, 0.005, false);
  if(!res.ok || res.transform.t.norm() > 0.01)
    return a;
  const Eigen::Matrix3d R = res.transform.R;
  std::vector<double> err;
  err.reserve(from.size());
  for(std::size_t i = 0; i + 1 < from.size(); i++)
    err.push_back(std::atan2((R * from[i]).cross(to[i]).norm(), (R * from[i]).dot(to[i])));
  std::nth_element(err.begin(), err.begin() + err.size() / 2, err.end());
  a.residualArcmin = err[err.size() / 2] * 180. / M_PI * 60.;
  a.angleDeg = Eigen::AngleAxisd(R).angle() * 180. / M_PI;
  a.inliers = res.inliers;
  for(int r = 0; r < 3; r++)
    for(int c = 0; c < 3; c++)
      a.rotation[r * 3 + c] = R(r, c);
  a.ok = true;
  return a;
}

void rotateDirections(std::vector<float>& m, const std::array<double, 9>& R)
{
  for(std::size_t i = 0; i + 3 < m.size(); i += 4)
  {
    if(!(m[i + 3] > 0.5f))
      continue;
    const double x = m[i], y = m[i + 1], z = m[i + 2];
    m[i] = float(R[0] * x + R[1] * y + R[2] * z);
    m[i + 1] = float(R[3] * x + R[4] * y + R[5] * z);
    m[i + 2] = float(R[6] * x + R[7] * y + R[8] * z);
  }
}

double projectorFitResidual(const std::vector<float>& points, int W, int H)
{
  std::vector<Corr> c;
  const int step = std::max(1, int(std::sqrt(double(W) * H / 2000.)));
  for(int y = 0; y < H; y += step)
    for(int x = 0; x < W; x += step)
      if(const float* p = &points[(std::size_t(y) * W + x) * 4]; p[3] > 0.5f)
        c.push_back({Eigen::Vector3d(p[0], p[1], p[2]), Eigen::Vector2d(x + 0.5, y + 0.5)});
  return fitResidual(c);
}

PoseFit refineCameraPose(
    const std::vector<DirectionMap>& maps, const RoomGeometry& room,
    const std::array<float, 3>& position, const std::array<float, 3>& rotation,
    bool refinePosition)
{
  PoseFit fit;
  fit.position = position;
  fit.rotation = rotation;
  if(room.empty())
    return fit;
  std::vector<Sampled> samples;
  for(const auto& m : maps)
  {
    if(!m.directions || m.width <= 0 || m.height <= 0)
      continue;
    Sampled s;
    const int step = std::max(1, int(std::sqrt(double(m.width) * m.height / 800.)));
    for(int y = 0; y < m.height; y += step)
      for(int x = 0; x < m.width; x += step)
        if(const float* d = &(*m.directions)[(std::size_t(y) * m.width + x) * 4]; d[3] > 0.5f)
        {
          s.dirs.push_back({d[0], d[1], d[2]});
          s.pix.emplace_back(x + 0.5, y + 0.5);
        }
    if(s.dirs.size() >= 50)
      samples.push_back(std::move(s));
  }
  auto castAll = [&](const Sampled& s, const Eigen::Matrix3d& R, const std::array<float, 3>& pos) {
    std::vector<Corr> c;
    c.reserve(s.dirs.size());
    for(std::size_t i = 0; i < s.dirs.size(); i++)
    {
      const Eigen::Vector3d d
          = (R * Eigen::Vector3d(s.dirs[i][0], s.dirs[i][1], s.dirs[i][2])).normalized();
      const std::array<float, 3> dd{float(d.x()), float(d.y()), float(d.z())};
      if(const float t = room.cast(pos, dd); t > 0.f)
        c.push_back({Eigen::Vector3d(pos[0], pos[1], pos[2]) + double(t) * d, s.pix[i]});
    }
    return c;
  };
  {
    const auto R0 = eulerRotation(rotation);
    std::vector<Sampled> constraining;
    for(auto& s : samples)
    {
      const auto c = castAll(s, R0, position);
      if(c.size() >= 12 && planar(c))
        fit.outputsPlanar++;
      else
        constraining.push_back(std::move(s));
    }
    samples = std::move(constraining);
  }
  fit.outputsUsed = int(samples.size());
  if(samples.empty())
  {
    fit.status = fit.outputsPlanar > 0
                     ? "the outputs each light one plane, which any camera pose explains: "
                       "they do not constrain the pose"
                     : "no output has enough measured directions";
    return fit;
  }

  auto cost = [&](const Eigen::Matrix<double, 6, 1>& v) {
    const std::array<float, 3> pos{float(v[0]), float(v[1]), float(v[2])};
    const Eigen::Matrix3d R = eulerRotation({float(v[3]), float(v[4]), float(v[5])});
    double total = 0.;
    for(const auto& s : samples)
    {
      const auto c = castAll(s, R, pos);
      const double r = c.size() < s.dirs.size() / 2 ? -1. : fitResidual(c);
      total += r < 0. ? 1e3 : r;
    }
    return total / double(samples.size());
  };

  Eigen::Matrix<double, 6, 1> x0;
  x0 << position[0], position[1], position[2], rotation[0], rotation[1], rotation[2];
  const Eigen::Matrix<double, 6, 1> start = x0;
  fit.residualBefore = cost(x0);
  double bestF = fit.residualBefore;
  Eigen::Matrix<double, 6, 1> bestX = x0;
  for(int restart = 0; restart < 5; restart++)
  {
  const double scale = restart == 0 ? 1. : std::pow(0.4, restart);
  const double steps[6]{0.15 * scale, 0.15 * scale, 0.15 * scale, 3. * scale, 3. * scale, 3. * scale};
  x0 = bestX;
  const int dims = 6;
  std::vector<Eigen::Matrix<double, 6, 1>> simplex(dims + 1, x0);
  std::vector<double> f(dims + 1);
  for(int i = 0; i < dims; i++)
    if(refinePosition || i >= 3)
      simplex[i + 1][i] += steps[i];
    else
      simplex[i + 1][i + 3] += steps[i + 3] * 0.5;
  for(int i = 0; i <= dims; i++)
    f[i] = cost(simplex[i]);
  auto clampPos = [&](Eigen::Matrix<double, 6, 1>& v) {
    if(!refinePosition)
      for(int i = 0; i < 3; i++)
        v[i] = start[i];
  };
  for(int it = 0; it < 400; it++)
  {
    std::vector<int> idx(dims + 1);
    for(int i = 0; i <= dims; i++)
      idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return f[a] < f[b]; });
    const int best = idx[0], worst = idx[dims], second = idx[dims - 1];
    if(std::abs(f[worst] - f[best]) < 1e-6 * std::max(1e-9, std::abs(f[best])) + 1e-9 && it > 40)
      break;
    Eigen::Matrix<double, 6, 1> c = Eigen::Matrix<double, 6, 1>::Zero();
    for(int i = 0; i <= dims; i++)
      if(i != worst)
        c += simplex[i];
    c /= double(dims);
    Eigen::Matrix<double, 6, 1> xr = c + (c - simplex[worst]);
    clampPos(xr);
    const double fr = cost(xr);
    if(fr < f[best])
    {
      Eigen::Matrix<double, 6, 1> xe = c + 2. * (c - simplex[worst]);
      clampPos(xe);
      const double fe = cost(xe);
      if(fe < fr)
      {
        simplex[worst] = xe;
        f[worst] = fe;
      }
      else
      {
        simplex[worst] = xr;
        f[worst] = fr;
      }
    }
    else if(fr < f[second])
    {
      simplex[worst] = xr;
      f[worst] = fr;
    }
    else
    {
      Eigen::Matrix<double, 6, 1> xc = c + 0.5 * (simplex[worst] - c);
      clampPos(xc);
      const double fc = cost(xc);
      if(fc < f[worst])
      {
        simplex[worst] = xc;
        f[worst] = fc;
      }
      else
        for(int i = 0; i <= dims; i++)
          if(i != best)
          {
            simplex[i] = simplex[best] + 0.5 * (simplex[i] - simplex[best]);
            clampPos(simplex[i]);
            f[i] = cost(simplex[i]);
          }
    }
  }
  const int best = int(std::min_element(f.begin(), f.end()) - f.begin());
  if(f[best] < bestF)
  {
    bestF = f[best];
    bestX = simplex[best];
  }
  }
  fit.residualAfter = bestF;
  for(int i = 0; i < 3; i++)
  {
    fit.position[i] = float(bestX[i]);
    fit.rotation[i] = float(bestX[i + 3]);
  }
  fit.ok = fit.residualBefore < 1e3 && fit.residualAfter < 0.95 * fit.residualBefore;
  if(!fit.ok)
  {
    fit.position = position;
    fit.rotation = rotation;
    fit.status = fit.residualBefore >= 1e3
                     ? "the outputs cannot be cast into the room from this pose"
                     : "no pose explains the outputs better than the given one";
  }
  else
    fit.status = "refined on " + std::to_string(fit.outputsUsed) + " output(s)";
  return fit;
}
}
