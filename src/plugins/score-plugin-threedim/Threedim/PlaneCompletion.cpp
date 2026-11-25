#include "PlaneCompletion.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <complex>

namespace Threedim
{
namespace
{
using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;

struct Sample
{
  Vec3 p, d;
};

double angle(const Mat3& H, const Vec3& p, const Vec3& d)
{
  const Vec3 q = H * p;
  const double n = q.norm();
  if(!(n > 1e-12))
    return M_PI;
  return std::acos(std::min(1., std::abs(q.dot(d)) / n));
}

void addRows(Eigen::Matrix<double, 9, 9>& N, const Vec3& p, const Vec3& d)
{
  Eigen::Matrix<double, 1, 9> r1, r2;
  r1 << 0, 0, 0, -d.z() * p.transpose(), d.y() * p.transpose();
  r2 << d.z() * p.transpose(), 0, 0, 0, -d.x() * p.transpose();
  N += r1.transpose() * r1 + r2.transpose() * r2;
}

Mat3 solve(const Eigen::Matrix<double, 9, 9>& N)
{
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double, 9, 9>> es(N);
  const Eigen::Matrix<double, 9, 1> h = es.eigenvectors().col(0);
  Mat3 H;
  H << h(0), h(1), h(2), h(3), h(4), h(5), h(6), h(7), h(8);
  return H;
}

Mat3 fit(const std::vector<Sample>& s, const std::vector<int>& idx)
{
  Eigen::Matrix<double, 9, 9> N = Eigen::Matrix<double, 9, 9>::Zero();
  for(int i : idx)
    addRows(N, s[i].p, s[i].d);
  Mat3 H = solve(N);
  double sign = 0.;
  for(int i : idx)
    sign += (H * s[i].p).dot(s[i].d);
  return sign < 0. ? Mat3(-H) : H;
}

//! The same sequence on every standard library, unlike std's distributions.
struct Rng
{
  uint64_t state{12345};
  uint64_t operator()() noexcept
  {
    uint64_t z = (state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
  }
  std::size_t below(std::size_t n) noexcept { return std::size_t((*this)() % n); }
};

double median(std::vector<double> v)
{
  if(v.empty())
    return 0.;
  std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
  return v[v.size() / 2];
}

//! The line of p where H1 p and H2 p are the same direction: the fixed line
//! of M = H2^-1 H1, spanned by the eigenvectors of its two closest eigenvalues.
bool cornerLine(const Mat3& H1, const Mat3& H2, Vec3& line)
{
  if(std::abs(H2.determinant()) < 1e-15)
    return false;
  const Mat3 M = H2.inverse() * H1;
  Eigen::EigenSolver<Mat3> es(M);
  if(es.info() != Eigen::Success)
    return false;
  const auto ev = es.eigenvalues();
  const auto V = es.eigenvectors();
  int a = 0, b = 1;
  double best = 1e300;
  for(int i = 0; i < 3; i++)
    for(int j = i + 1; j < 3; j++)
      if(const double dist = std::abs(ev(i) - ev(j)); dist < best)
      {
        best = dist;
        a = i;
        b = j;
      }
  Vec3 e1, e2;
  if(std::abs(ev(a).imag()) > 1e-9 * std::max(1., std::abs(ev(a))))
  {
    e1 = V.col(a).real();
    e2 = V.col(a).imag();
  }
  else
  {
    e1 = V.col(a).real();
    e2 = V.col(b).real();
  }
  line = e1.cross(e2);
  return line.norm() > 1e-12;
}
}

namespace
{
struct Fitted
{
  std::vector<Mat3> models;
  std::vector<std::array<double, 9>> homographies;
  std::vector<int> inliers;
  std::vector<double> medianArcmin;
};

std::vector<Sample> sampleMap(
    const std::vector<float>& dir, int W, int H, int grid)
{
  std::vector<Sample> samples;
  const double S = std::max(W, H);
  for(int y = 0; y < H; y += grid)
    for(int x = 0; x < W; x += grid)
    {
      const float* q = &dir[(std::size_t(y) * W + x) * 4];
      if(!(q[3] > 0.5f) || !std::isfinite(q[0]) || !std::isfinite(q[1]) || !std::isfinite(q[2]))
        continue;
      Vec3 d(q[0], q[1], q[2]);
      const double l = d.norm();
      if(!(l > 1e-9))
        continue;
      samples.push_back({Vec3((x - W / 2.) / S, (y - H / 2.) / S, 1.), d / l});
    }
  return samples;
}

Fitted fitPlanes(const std::vector<Sample>& samples, double thr, int maxPlanes)
{
  Fitted res;
  Rng rng;
  std::vector<int> remaining(samples.size());
  for(std::size_t i = 0; i < samples.size(); i++)
    remaining[i] = int(i);
  const std::size_t minLeft
      = std::max(samples.size() / 25, std::min<std::size_t>(400, samples.size() / 8));
  std::vector<Mat3> models;
  while((int)res.models.size() < maxPlanes && remaining.size() >= 8
        && (res.models.empty() || remaining.size() >= minLeft))
  {
    std::vector<int> scoring = remaining;
    if(scoring.size() > 20000)
    {
      for(std::size_t k = 0; k < 20000; k++)
        std::swap(scoring[k], scoring[k + rng.below(scoring.size() - k)]);
      scoring.resize(20000);
    }
    std::size_t bestScore = 0;
    Mat3 bestModel = Mat3::Identity();
    for(int it = 0; it < 1500; it++)
    {
      std::vector<int> four;
      while(four.size() < 4)
        if(const int k = remaining[rng.below(remaining.size())]; std::find(four.begin(), four.end(), k) == four.end())
          four.push_back(k);
      const Mat3 Hm = fit(samples, four);
      std::size_t score = 0;
      for(int k : scoring)
        score += angle(Hm, samples[k].p, samples[k].d) < thr;
      if(score > bestScore)
      {
        bestScore = score;
        bestModel = Hm;
      }
    }
    std::vector<int> best;
    for(int k : remaining)
      if(angle(bestModel, samples[k].p, samples[k].d) < thr)
        best.push_back(k);
    if(best.size() < 8 || (!res.models.empty() && best.size() < minLeft))
      break;
    Mat3 Hm = fit(samples, best);
    for(int round = 0; round < 4; round++)
    {
      std::vector<int> in;
      for(int k : remaining)
        if(angle(Hm, samples[k].p, samples[k].d) < thr)
          in.push_back(k);
      if(in.size() < 8)
        break;
      best = std::move(in);
      Hm = fit(samples, best);
    }
    std::vector<double> err;
    for(int k : best)
      err.push_back(angle(Hm, samples[k].p, samples[k].d));
    res.models.push_back(Hm);
    res.inliers.push_back(int(best.size()));
    res.medianArcmin.push_back(median(err) * 180. / M_PI * 60.);
    std::array<double, 9> h;
    for(int r = 0; r < 3; r++)
      for(int c = 0; c < 3; c++)
        h[r * 3 + c] = Hm(r, c);
    res.homographies.push_back(h);
    std::vector<int> rest;
    std::sort(best.begin(), best.end());
    for(int k : remaining)
      if(!std::binary_search(best.begin(), best.end(), k))
        rest.push_back(k);
    remaining = std::move(rest);
  }
  return res;
}
}

double planeResidual(
    const std::vector<float>& dir, int W, int H, double thresholdArcmin, int maxPlanes)
{
  const std::size_t n = std::size_t(W) * H;
  if(W <= 0 || H <= 0 || dir.size() < n * 4)
    return -1.;
  const double thr = thresholdArcmin / 60. * M_PI / 180.;
  const auto samples = sampleMap(dir, W, H, 6);
  if(samples.size() < 8)
    return -1.;
  const auto fit = fitPlanes(samples, thr, std::clamp(maxPlanes, 1, 2));
  double cost = 0.;
  for(const auto& s : samples)
  {
    double e = M_PI;
    for(const auto& m : fit.models)
      e = std::min(e, angle(m, s.p, s.d));
    const double t = std::min(e, thr) / thr;
    cost += t * t;
  }
  return cost / double(samples.size());
}

PlaneCompletion completeFromPlanes(
    const std::vector<float>& dir, int W, int H, double thresholdArcmin, int maxPlanes)
{
  maxPlanes = std::clamp(maxPlanes, 1, 2);
  PlaneCompletion out;
  out.width = W;
  out.height = H;
  const std::size_t n = std::size_t(W) * H;
  if(W <= 0 || H <= 0 || dir.size() < n * 4)
  {
    out.status = "no direction map";
    return out;
  }
  const double S = std::max(W, H);
  const double thr = thresholdArcmin / 60. * M_PI / 180.;
  auto pix = [&](int x, int y) { return Vec3((x - W / 2.) / S, (y - H / 2.) / S, 1.); };
  auto measured = [&](std::size_t i, Vec3& d) {
    const float* q = &dir[i * 4];
    if(!(q[3] > 0.5f) || !std::isfinite(q[0]) || !std::isfinite(q[1]) || !std::isfinite(q[2]))
      return false;
    d = Vec3(q[0], q[1], q[2]);
    const double l = d.norm();
    if(!(l > 1e-9))
      return false;
    d /= l;
    return true;
  };

  const auto samples = sampleMap(dir, W, H, 6);
  if(samples.size() < 8)
  {
    out.status = "too few measured pixels to fit a plane";
    return out;
  }

  const auto fit = fitPlanes(samples, thr, maxPlanes);
  const auto& models = fit.models;
  out.homographies = fit.homographies;
  out.inliers = fit.inliers;
  out.medianArcmin = fit.medianArcmin;
  if(models.empty())
  {
    out.status = "no plane fits the measured directions";
    return out;
  }

  // Which model each pixel takes: one plane, or a side of the corner line.
  Vec3 line = Vec3::Zero();
  int sidePlus = 0, sideMinus = 0;
  if(models.size() >= 2 && cornerLine(models[0], models[1], line))
  {
    const double tol = 12. / 60. * M_PI / 180.;
    int agreeA = 0, agreeB = 0;
    for(int y = 0; y < H; y += 2)
      for(int x = 0; x < W; x += 2)
        if(Vec3 d; measured(std::size_t(y) * W + x, d))
        {
          const Vec3 p = pix(x, y);
          const bool plus = line.dot(p) >= 0.;
          agreeA += angle(models[plus ? 0 : 1], p, d) < tol;
          agreeB += angle(models[plus ? 1 : 0], p, d) < tol;
        }
    sidePlus = agreeA >= agreeB ? 0 : 1;
    sideMinus = 1 - sidePlus;
  }
  else if(models.size() >= 2)
  {
    out.status = "two planes found, but no room corner between them";
    return out;
  }

  out.directions.assign(n * 4, 0.f);
  std::size_t total = 0, explained = 0;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const std::size_t i = std::size_t(y) * W + x;
      const Vec3 p = pix(x, y);
      const int m = line.isZero() ? 0 : (line.dot(p) >= 0. ? sidePlus : sideMinus);
      Vec3 q = models[m] * p;
      q.normalize();
      out.directions[i * 4 + 0] = float(q.x());
      out.directions[i * 4 + 1] = float(q.y());
      out.directions[i * 4 + 2] = float(q.z());
      out.directions[i * 4 + 3] = 1.f;
      if(Vec3 d; measured(i, d))
      {
        total++;
        explained += std::acos(std::min(1., std::abs(q.dot(d)))) < 2. * thr;
      }
    }
  out.explained = total ? double(explained) / double(total) : 0.;
  out.status = std::to_string(models.size()) + " plane(s), "
               + std::to_string(int(std::lround(100. * out.explained)))
               + "% of the measured pixels explained";
  out.ok = out.explained >= 0.8;
  if(!out.ok)
    out.status += ": not flat surfaces, the measured map is kept";
  return out;
}
}
