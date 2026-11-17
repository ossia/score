#include "PoseRefinement.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>

namespace Threedim
{
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
  Eigen::Vector3d mx = Eigen::Vector3d::Zero();
  Eigen::Vector2d mp = Eigen::Vector2d::Zero();
  for(const auto& k : c)
  {
    mx += k.X;
    mp += k.p;
  }
  mx /= double(c.size());
  mp /= double(c.size());
  double sx = 0., sp = 0.;
  for(const auto& k : c)
  {
    sx += (k.X - mx).norm();
    sp += (k.p - mp).norm();
  }
  sx = std::sqrt(3.) / std::max(1e-12, sx / double(c.size()));
  sp = std::sqrt(2.) / std::max(1e-12, sp / double(c.size()));
  Eigen::Matrix<double, 12, 12> N = Eigen::Matrix<double, 12, 12>::Zero();
  for(const auto& k : c)
  {
    const Eigen::Vector4d X((k.X.x() - mx.x()) * sx, (k.X.y() - mx.y()) * sx,
                            (k.X.z() - mx.z()) * sx, 1.);
    const double u = (k.p.x() - mp.x()) * sp, v = (k.p.y() - mp.y()) * sp;
    Eigen::Matrix<double, 1, 12> r1, r2;
    r1 << X.transpose(), Eigen::RowVector4d::Zero(), -u * X.transpose();
    r2 << Eigen::RowVector4d::Zero(), X.transpose(), -v * X.transpose();
    N += r1.transpose() * r1 + r2.transpose() * r2;
  }
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double, 12, 12>> es(N);
  const Eigen::Matrix<double, 12, 1> h = es.eigenvectors().col(0);
  Eigen::Matrix<double, 3, 4> P;
  P << h(0), h(1), h(2), h(3), h(4), h(5), h(6), h(7), h(8), h(9), h(10), h(11);
  double e2 = 0.;
  for(const auto& k : c)
  {
    const Eigen::Vector4d X((k.X.x() - mx.x()) * sx, (k.X.y() - mx.y()) * sx,
                            (k.X.z() - mx.z()) * sx, 1.);
    const Eigen::Vector3d q = P * X;
    if(std::abs(q.z()) < 1e-12)
      return 1e6;
    const Eigen::Vector2d pr(q.x() / q.z() / sp + mp.x(), q.y() / q.z() / sp + mp.y());
    e2 += (pr - k.p).squaredNorm();
  }
  return std::sqrt(e2 / double(c.size()));
}

struct Sampled
{
  std::vector<std::array<float, 3>> dirs;
  std::vector<Eigen::Vector2d> pix;
};
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
  fit.outputsUsed = int(samples.size());
  if(samples.empty())
    return fit;

  auto cost = [&](const Eigen::Matrix<double, 6, 1>& v) {
    const std::array<float, 3> pos{float(v[0]), float(v[1]), float(v[2])};
    const std::array<float, 3> rot{float(v[3]), float(v[4]), float(v[5])};
    const Eigen::Matrix3d R
        = (Eigen::AngleAxisd(v[4] * M_PI / 180., Eigen::Vector3d::UnitY())
           * Eigen::AngleAxisd(v[3] * M_PI / 180., Eigen::Vector3d::UnitX())
           * Eigen::AngleAxisd(v[5] * M_PI / 180., Eigen::Vector3d::UnitZ()))
              .toRotationMatrix();
    double total = 0.;
    for(const auto& s : samples)
    {
      std::vector<Corr> c;
      c.reserve(s.dirs.size());
      for(std::size_t i = 0; i < s.dirs.size(); i++)
      {
        Eigen::Vector3d d = R * Eigen::Vector3d(s.dirs[i][0], s.dirs[i][1], s.dirs[i][2]);
        d.normalize();
        const std::array<float, 3> dd{float(d.x()), float(d.y()), float(d.z())};
        const float t = room.cast(pos, dd);
        if(t > 0.f)
          c.push_back({Eigen::Vector3d(pos[0], pos[1], pos[2]) + double(t) * d, s.pix[i]});
      }
      if(c.size() < s.dirs.size() / 2)
        total += 1e3;
      else
      {
        const double r = fitResidual(c);
        total += r < 0. ? 1e3 : r;
      }
    }
    (void)rot;
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
  fit.ok = fit.residualAfter <= fit.residualBefore;
  return fit;
}
}
