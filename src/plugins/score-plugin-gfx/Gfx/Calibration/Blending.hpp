#pragma once
#include <score_plugin_gfx_export.h>
#include <Gfx/Calibration/ProjectorModel.hpp>

#include <Eigen/Cholesky>
#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Multi-projector blending (alpha) and black-level (beta) maps from per
// projector XYZ maps in one world frame (maps RGBA32F rows top to bottom,
// w > 0 where valid; alpha and beta in linear light:
// out = content * alpha + beta).
//
//  - Geometric weights: distance of the pixel to the edge of its projector's
//    lit area (image border or valid-mask border), normalised over the
//    projectors that light the same surface point (Raskar et al. 1999).
//  - Luminance: the achievable white W0 = B + sum_j b_j L_j and the black B are
//    replaced by the largest W' <= W0 and smallest B' >= B whose log-luminance
//    gradient is bounded (Majumder & Stevens 2005, constrained gradient), a
//    zero bound being the flat LAM of Majumder & Stevens 2002.
//  - Per channel (RGB lum / black maps): the white of each channel is bounded
//    separately, W'_c = k_c Y' with k the common white chromaticity and Y' the
//    constrained luminance under min_c W0_c / k_c, the black B'_c per channel;
//    alpha and beta are then RGB (Majumder & Stevens 2004, per channel
//    luminance; Majumder et al. 2000, common white).
namespace score::gfx::blend
{
using Vec3 = Eigen::Vector3d;
using Vec2 = Eigen::Vector2d;

struct Projector
{
  int width{}, height{};
  //! RGBA, rows top to bottom, w > 0 = valid.
  std::vector<float> xyz;
  //! Pinhole model (world -> pixels); without it the XYZ map is inverted by search.
  procam::Model model;
  bool hasModel{};
  //! Optional RGB per pixel: measured white - black, and black, linear light.
  std::vector<float> lum;
  std::vector<float> black;

  //! Ramp width in pixels from the edge of the lit area; 0 = the raw distance.
  float rampWidth{0.f};
  //! Weight = ramp ^ exponent.
  float exponent{1.f};

  bool valid(int i) const noexcept { return xyz[std::size_t(i) * 4 + 3] > 0.f; }
  Vec3 point(int i) const noexcept
  {
    const float* p = xyz.data() + std::size_t(i) * 4;
    return {p[0], p[1], p[2]};
  }
};

struct Options
{
  //! A projector lights a point when its own map, at the pixel the point
  //! projects to, is within this distance of it (occlusion test).
  double visibilityTolerance{0.05};
  //! Added to the tolerance: this many pixel footprints of the other projector.
  double footprintPixels{4.};
  //! Use Projector::lum / black when present.
  bool photometric{true};
  //! Black of a projector without a black map, as a fraction of its white.
  float blackLevel{0.f};
  bool luminanceUniformity{false};
  bool blackCompensation{true};
  //! RGB alpha and beta when every projector has RGB lum data.
  bool perChannel{false};
  //! Bound on |d log L| per radian seen from the viewer (or per metre without
  //! one); 0 flattens to the dimmest point.
  double maxLogSlope{2.86};
  bool hasViewer{};
  Vec3 viewer{0., 1.6, 0.};
  int threads{0};
};

struct Result
{
  //! Per projector, `channels` floats per pixel (1 grey, 3 RGB).
  std::vector<std::vector<float>> alpha;
  std::vector<std::vector<float>> beta;
  int channels{1};
  //! Per channel mode: the common white, luma 1.
  std::array<float, 3> whiteChroma{1.f, 1.f, 1.f};
  //! Per projector: pixels lit by at least two projectors / lit pixels.
  std::vector<float> overlap;
  //! Per projector RGB gains matching the white points (1 without RGB data).
  std::vector<std::array<float, 3>> colorGain;
  float blackTarget{};
  float whiteTarget{};
  std::string status;
};

namespace detail
{
//! Felzenszwalb & Huttenlocher 2012: squared distance to the nearest site
//! (f = 0) of a sampled function, one dimension.
SCORE_PLUGIN_GFX_EXPORT void edt1d(const float* f, int n, float* d, int* v, float* z);

template <typename F>
void parallelFor(int n, int threads, F&& f)
{
  if(threads <= 0)
    threads = std::max(1u, std::thread::hardware_concurrency());
  threads = std::clamp(threads, 1, std::max(1, n));
  if(threads == 1)
  {
    for(int i = 0; i < n; i++)
      f(i);
    return;
  }
  std::vector<std::thread> pool;
  for(int t = 0; t < threads; t++)
    pool.emplace_back([&, t] {
      for(int i = t; i < n; i += threads)
        f(i);
    });
  for(auto& th : pool)
    th.join();
}

inline float luma(const float* rgb) noexcept
{
  return 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
}
}

//! Euclidean distance in pixels from each lit pixel to the nearest site, -1 on
//! unlit pixels. Without sites given, the sites are the unlit pixels and the
//! image border and the distance is minus one, 0 on the outermost lit pixels;
//! with sites and none set, every lit pixel gets 1e6.
inline std::vector<float>
distanceToEdge(const Projector& p, const std::vector<uint8_t>* sites = nullptr)
{
  const int W = p.width + 2, H = p.height + 2;
  const float inf = 1e20f;
  std::vector<float> g(std::size_t(W) * H, sites ? inf : 0.f);
  bool any = !sites;
  for(int y = 0; y < p.height; y++)
    for(int x = 0; x < p.width; x++)
    {
      const int i = y * p.width + x;
      float& c = g[std::size_t(y + 1) * W + x + 1];
      if(sites)
      {
        c = (*sites)[i] ? 0.f : inf;
        any |= (*sites)[i] != 0;
      }
      else
        c = p.valid(i) ? inf : 0.f;
    }

  std::vector<float> out(std::size_t(p.width) * p.height, -1.f);
  if(!any)
  {
    for(std::size_t i = 0; i < out.size(); i++)
      if(p.valid(int(i)))
        out[i] = 1e6f;
    return out;
  }
  const int n = std::max(W, H);
  std::vector<float> f(n), d(n), z(n + 1);
  std::vector<int> v(n);
  for(int x = 0; x < W; x++)
  {
    for(int y = 0; y < H; y++)
      f[y] = g[std::size_t(y) * W + x];
    detail::edt1d(f.data(), H, d.data(), v.data(), z.data());
    for(int y = 0; y < H; y++)
      g[std::size_t(y) * W + x] = d[y];
  }
  for(int y = 0; y < H; y++)
  {
    detail::edt1d(g.data() + std::size_t(y) * W, W, d.data(), v.data(), z.data());
    std::copy_n(d.data(), W, g.data() + std::size_t(y) * W);
  }
  for(int y = 0; y < p.height; y++)
    for(int x = 0; x < p.width; x++)
    {
      const int i = y * p.width + x;
      if(!p.valid(i))
        continue;
      const float s = std::sqrt(g[std::size_t(y + 1) * W + x + 1]);
      out[i] = sites ? s : s - 1.f;
    }
  return out;
}

//! The blend weight of a pixel from its distance to the edge.
SCORE_PLUGIN_GFX_EXPORT float rampWeight(float dist, float rampWidth, float exponent) noexcept;

SCORE_PLUGIN_GFX_EXPORT float sampleBilinear(
    const std::vector<float>& m, int W, int H, double u, double v, int C = 1,
    int c = 0) noexcept;

//! World point -> pixel of a projector, through its pinhole model or, without
//! one, by inverting its XYZ map: nearest coarse sample, then Gauss-Newton on
//! the bilinearly interpolated map.
class Reprojector
{
public:
  explicit Reprojector(const Projector& p, int step = 8)
      : m_p{p}
  {
    if(p.hasModel)
      return;
    m_step = step;
    std::vector<Vec3> pts;
    double spacing = 0.;
    int ns = 0;
    for(int y = 0; y < p.height; y += step)
      for(int x = 0; x < p.width; x += step)
      {
        const int i = y * p.width + x;
        if(!p.valid(i))
          continue;
        m_samples.push_back({x, y});
        pts.push_back(p.point(i));
        if(x + step < p.width && p.valid(i + step))
        {
          spacing += (p.point(i + step) - p.point(i)).norm();
          ns++;
        }
      }
    m_cell = ns > 0 ? std::max(1e-6, 2. * spacing / ns) : 1.;
    for(std::size_t k = 0; k < pts.size(); k++)
      m_grid[key(pts[k])].push_back(int(k));
    m_points = std::move(pts);
  }

  //! Pixel (u, v) of the projector at X, without visibility test.
  bool find(const Vec3& X, Vec2& px) const
  {
    if(m_p.hasModel)
      return m_p.model.project(X, px);
    const auto c = cell(X);
    double best = std::numeric_limits<double>::infinity();
    int bi = -1;
    for(int dz = -1; dz <= 1; dz++)
      for(int dy = -1; dy <= 1; dy++)
        for(int dx = -1; dx <= 1; dx++)
        {
          auto it = m_grid.find(pack(c[0] + dx, c[1] + dy, c[2] + dz));
          if(it == m_grid.end())
            continue;
          for(int k : it->second)
          {
            const double d = (m_points[k] - X).squaredNorm();
            if(d < best)
            {
              best = d;
              bi = k;
            }
          }
        }
    if(bi < 0)
      return false;
    Vec2 p(m_samples[bi][0], m_samples[bi][1]);
    for(int it = 0; it < 10; it++)
    {
      Vec2 step;
      if(!newtonStep(p, X, step))
        break;
      const double n = step.norm();
      const double lim = 2. * m_step;
      const Vec2 q = p + (n > lim ? Vec2(step * (lim / n)) : step);
      const Vec2 c(
          std::clamp(q.x(), 0., double(m_p.width - 1)),
          std::clamp(q.y(), 0., double(m_p.height - 1)));
      if(c != q && (c - p).norm() < 1e-3)
      {
        p = q;
        break;
      }
      p = c;
      if(n < 1e-3)
        break;
    }
    px = p;
    return true;
  }

  //! Gauss-Newton step towards X with one-sided differences of the map.
  bool newtonStep(Vec2 p, const Vec3& X, Vec2& step) const
  {
    p.x() = std::clamp(p.x(), 0., double(m_p.width - 1));
    p.y() = std::clamp(p.y(), 0., double(m_p.height - 1));
    Vec3 f, a, b;
    if(!at(p.x(), p.y(), f))
      return false;
    Eigen::Matrix<double, 3, 2> J;
    if(at(p.x() + 1., p.y(), a))
      J.col(0) = a - f;
    else if(at(p.x() - 1., p.y(), a))
      J.col(0) = f - a;
    else
      return false;
    if(at(p.x(), p.y() + 1., b))
      J.col(1) = b - f;
    else if(at(p.x(), p.y() - 1., b))
      J.col(1) = f - b;
    else
      return false;
    const Eigen::Matrix2d JtJ = J.transpose() * J;
    if(std::abs(JtJ.determinant()) < 1e-30)
      return false;
    step = JtJ.ldlt().solve(J.transpose() * (X - f));
    return step.allFinite();
  }

  //! Bilinear XYZ at (u, v) when the four neighbours are lit.
  bool at(double u, double v, Vec3& out) const noexcept
  {
    const int W = m_p.width, H = m_p.height;
    if(u < 0. || v < 0. || u > W - 1 || v > H - 1)
      return false;
    const int x0 = std::min(int(u), std::max(0, W - 2)), y0 = std::min(int(v), std::max(0, H - 2));
    const int x1 = std::min(x0 + 1, W - 1), y1 = std::min(y0 + 1, H - 1);
    const int i00 = y0 * W + x0, i10 = y0 * W + x1, i01 = y1 * W + x0, i11 = y1 * W + x1;
    if(!m_p.valid(i00) || !m_p.valid(i10) || !m_p.valid(i01) || !m_p.valid(i11))
      return false;
    const double fx = u - x0, fy = v - y0;
    out = (m_p.point(i00) * (1. - fx) + m_p.point(i10) * fx) * (1. - fy)
          + (m_p.point(i01) * (1. - fx) + m_p.point(i11) * fx) * fy;
    return true;
  }

private:
  std::array<int64_t, 3> cell(const Vec3& X) const noexcept
  {
    auto c = [&](double v) {
      const double q = std::floor(v / m_cell);
      return q > -1e15 ? int64_t(std::min(q, 1e15)) : int64_t(-1e15);
    };
    return {c(X.x()), c(X.y()), c(X.z())};
  }
  static uint64_t pack(int64_t x, int64_t y, int64_t z) noexcept
  {
    return (uint64_t(x & 0x1fffff) << 42) | (uint64_t(y & 0x1fffff) << 21)
           | uint64_t(z & 0x1fffff);
  }
  uint64_t key(const Vec3& X) const noexcept
  {
    const auto c = cell(X);
    return pack(c[0], c[1], c[2]);
  }

  const Projector& m_p;
  int m_step{8};
  double m_cell{1.};
  std::vector<std::array<int, 2>> m_samples;
  std::vector<Vec3> m_points;
  std::unordered_map<uint64_t, std::vector<int>> m_grid;
};

//! Where projector j lights the world point X seen from a pixel of another
//! projector: its pixel, if j's own map agrees at that pixel (visibility).
SCORE_PLUGIN_GFX_EXPORT bool lights(
    const Projector& pj, const Reprojector& rj, const Vec3& X, const Options& opt, Vec2& px);

//! The outermost lit pixels of projector i beyond which another projector
//! keeps lighting the surface: the edges its weight ramps down to. An edge on
//! the outer border of the whole display does not attenuate.
SCORE_PLUGIN_GFX_EXPORT std::vector<uint8_t> innerEdges(
    const std::vector<Projector>& P, const std::vector<const Reprojector*>& rep, int i,
    const Options& opt);

namespace detail
{
struct Link
{
  uint32_t pixel;
  uint32_t other;
  uint32_t projector;
};

//! Bounds |log f(p) - log f(q)| by slope * dist(p, q) on each projector's
//! 8-connected grid and keeps f equal across the links: the largest f below
//! (upper = true: the smallest f above) the input, exactly, as a multi-source
//! shortest path in log f.
SCORE_PLUGIN_GFX_EXPORT void constrainSlope(
    std::vector<std::vector<float>>& f, const std::vector<Projector>& P,
    const std::vector<std::vector<Link>>& links, double slope, const Options& opt,
    bool upper);
}

SCORE_PLUGIN_GFX_EXPORT Result compute(std::vector<Projector>& P, const Options& opt);
}
