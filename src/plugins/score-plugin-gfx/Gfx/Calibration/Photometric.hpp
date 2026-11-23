#pragma once
#include <score_plugin_gfx_export.h>
#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Photometric calibration of a projector-camera pair. Colours are linear RGB
// in [0, 1], images RGBA32F rows top to bottom.
//  - Camera response: Debevec & Malik 1997, g(Z) = ln E + ln dt from pixels
//    seen at several exposures, least squares with a weighted second
//    derivative smoothness term and the hat weighting, g(Zmid) = 0.
//  - Projector response: per channel, normalised f(0) = 0, f(1) = 1, from a
//    ramp of uniform levels measured by the linearised camera (Majumder &
//    Stevens 2004; Brown, Majumder & Yang 2005), made monotonic by isotonic
//    regression and interpolated with a monotone cubic (Fritsch & Carlson 1980).
//  - Radiometric compensation: per pixel C = V P + F (Nayar et al. 2003), V
//    from the red, green and blue primaries: compensationMatrix gives
//    M = V^-1 diag(k), and P = s M c for a content c. The black offset F is
//    not inverted here (the blend's black level handles it), and the caller
//    chooses the global target k.
//  - The projector's own colour mixing (a DLP white segment, a picture mode
//    that boosts the primaries): measured on a grid of uniform colours and
//    inverted through a 3D table.
namespace score::gfx::photometric
{

//! g per channel over `levels` codes (Z = round(v * (levels - 1))).
struct CameraResponse
{
  int levels{256};
  std::array<std::vector<double>, 3> g;

  bool valid() const noexcept
  {
    return levels >= 2 && g[0].size() == std::size_t(levels)
           && g[1].size() == std::size_t(levels) && g[2].size() == std::size_t(levels);
  }

  //! Relative radiance of camera value v in [0, 1], 1 at v = 1.
  double linear(int c, double v) const noexcept
  {
    if(c < 0 || c > 2 || !valid())
      return v > 0. ? std::min(v, 1.) : 0.;
    const auto& gc = g[c];
    const double z = (v > 0. ? std::min(v, 1.) : 0.) * (levels - 1);
    const int z0 = std::min(int(z), levels - 2);
    const double t = z - z0;
    const double gz = gc[z0] * (1. - t) + gc[z0 + 1] * t;
    return std::exp(gz - gc[levels - 1]);
  }
};

//! Whether g rises over the middle codes, by at least `minSpan` (ln radiance),
//! without falling back by more than `tolerance` anywhere in between. A bracket
//! whose frames did not change with the exposure solves to a flat g. The
//! output of solveCameraResponse is forced to rise, so on it only the span
//! can fail; the fall-back test is for responses loaded from elsewhere.
SCORE_PLUGIN_GFX_EXPORT bool plausibleResponse(
    const CameraResponse& r, double minSpan, double tolerance = 0.1) noexcept;

//! Debevec & Malik's hat: z - Zmin below the middle, Zmax - z above.
inline double hatWeight(int z, int levels) noexcept
{
  const int zmax = levels - 1;
  return z <= zmax / 2 ? double(z) : double(zmax - z);
}

//! One channel. Z[i * P + j]: code of sample i at exposure j, < 0 = missing.
//! Returns g (levels values, g(levels / 2) = 0), and the ln E of each sample;
//! empty when the input is inconsistent or the solve fails.
SCORE_PLUGIN_GFX_EXPORT std::vector<double> solveCameraResponse(
    const std::vector<int>& Z, int samples, const std::vector<double>& lnDt, int levels,
    double lambda, std::vector<double>* lnE = nullptr);

//! Pool adjacent violators: the non-decreasing least-squares fit of y.
SCORE_PLUGIN_GFX_EXPORT std::vector<double> isotonic(const std::vector<double>& y, const std::vector<double>& w);

//! Monotone piecewise cubic Hermite interpolation through increasing (x, y).
class MonotoneCubic
{
public:
  MonotoneCubic() = default;
  MonotoneCubic(std::vector<double> x, std::vector<double> y)
      : m_x{std::move(x)}
      , m_y{std::move(y)}
  {
    const std::size_t n = std::min(m_x.size(), m_y.size());
    m_x.resize(n);
    m_y.resize(n);
    m_m.assign(n, 0.);
    if(n < 2)
      return;
    std::vector<double> d(n - 1);
    for(std::size_t k = 0; k + 1 < n; k++)
      d[k] = (m_y[k + 1] - m_y[k]) / std::max(1e-12, m_x[k + 1] - m_x[k]);
    m_m[0] = d[0];
    m_m[n - 1] = d[n - 2];
    for(std::size_t k = 1; k + 1 < n; k++)
      m_m[k] = d[k - 1] * d[k] <= 0. ? 0. : 0.5 * (d[k - 1] + d[k]);
    for(std::size_t k = 0; k + 1 < n; k++)
    {
      if(d[k] == 0.)
      {
        m_m[k] = m_m[k + 1] = 0.;
        continue;
      }
      const double a = m_m[k] / d[k], b = m_m[k + 1] / d[k];
      const double s = a * a + b * b;
      if(s > 9.)
      {
        const double t = 3. / std::sqrt(s);
        m_m[k] = t * a * d[k];
        m_m[k + 1] = t * b * d[k];
      }
    }
  }

  double operator()(double x) const noexcept
  {
    const std::size_t n = m_x.size();
    if(n == 0)
      return x;
    if(!(x > m_x.front()))
      return m_y.front();
    if(x >= m_x.back())
      return m_y.back();
    const std::size_t k
        = std::size_t(std::upper_bound(m_x.begin(), m_x.end(), x) - m_x.begin()) - 1;
    const double h = m_x[k + 1] - m_x[k];
    const double t = (x - m_x[k]) / h;
    const double t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * m_y[k] + (t3 - 2 * t2 + t) * h * m_m[k]
           + (-2 * t3 + 3 * t2) * m_y[k + 1] + (t3 - t2) * h * m_m[k + 1];
  }

private:
  std::vector<double> m_x, m_y, m_m;
};

//! Normalised response of one channel from measured levels.
struct ChannelResponse
{
  //! Input codes, 0 first, 1 last.
  std::vector<double> x;
  //! Measured output, normalised: y(0) = 0, y(1) = 1.
  std::vector<double> measured;
  //! The monotone fit at x.
  std::vector<double> fitted;
  //! Best single power y = x ^ gamma.
  double gamma{2.2};
  MonotoneCubic curve;
  //! False when the measurements could not be fitted: the curve is then the identity.
  bool valid{};

  double operator()(double v) const noexcept { return curve(std::clamp(v, 0., 1.)); }
  //! Code giving the normalised output t, by bisection.
  double inverse(double t) const noexcept
  {
    t = std::clamp(t, 0., 1.);
    double lo = 0., hi = 1.;
    for(int it = 0; it < 40; it++)
    {
      const double mid = 0.5 * (lo + hi);
      (curve(mid) < t ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
  }
};

//! x: increasing codes in (0, 1], the last 1; m: the measured light at each
//! code minus the black, any unit; the black itself is the point (0, 0).
SCORE_PLUGIN_GFX_EXPORT ChannelResponse fitChannelResponse(const std::vector<double>& x, const std::vector<double>& m);

//! The scalar a channel's light is measured as: the projection of the camera
//! colour (minus black) on that channel's full-level colour.
inline double projectOn(const std::array<double, 3>& v, const std::array<double, 3>& dir) noexcept
{
  const double n = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
  if(n <= 0.)
    return 0.;
  return (v[0] * dir[0] + v[1] * dir[1] + v[2] * dir[2]) / std::sqrt(n);
}

//! Per pixel: V (camera from linear projector RGB, column c = primary c minus
//! black) -> M = V^-1 diag(k), so that P = s M c makes the camera see s k c.
//! Returns false (M = identity) when V is not invertible.
SCORE_PLUGIN_GFX_EXPORT bool compensationMatrix(
    const Eigen::Matrix3d& V, const std::array<double, 3>& k, Eigen::Matrix3d& M,
    double maxCondition = 1e3) noexcept;

//! The largest s with s M (1, 1, 1) <= 1: the brightest white this pixel can
//! show.
inline double maxWhiteScale(const Eigen::Matrix3d& M) noexcept
{
  const Eigen::Vector3d w = M * Eigen::Vector3d::Ones();
  const double m = w.maxCoeff();
  return m > 0. ? 1. / m : 0.;
}

//! The projector's own colour mixing, global: the light shown for per-channel
//! linear light l, in units of the full primaries (identity when additive).
//! Sampled on a grid of codes, interpolated tetrahedrally, so that what
//! changes across equal codes (grey) stays on cell faces.
struct SCORE_PLUGIN_GFX_EXPORT ColourMixing
{
  //! Per channel, the linear light at the grid codes, increasing, 0 first.
  std::array<std::vector<double>, 3> axis;
  //! n^3 samples, red fastest, then green, then blue.
  std::vector<std::array<double, 3>> shown;

  int n() const noexcept { return int(axis[0].size()); }
  bool valid() const noexcept
  {
    const int N = n();
    return N >= 2 && axis[1].size() == axis[0].size() && axis[2].size() == axis[0].size()
           && shown.size() == std::size_t(N) * N * N;
  }

  //! J: the derivative of the result in l, on the tetrahedron l lies in.
  std::array<double, 3>
  forward(const std::array<double, 3>& l, Eigen::Matrix3d* J = nullptr) const noexcept
  {
    const int N = n();
    int k[3];
    double t[3], dt[3];
    for(int c = 0; c < 3; c++)
    {
      const auto& a = axis[c];
      const double v = l[c] > a.front() ? std::min(l[c], a.back()) : a.front();
      k[c] = std::clamp(
          int(std::upper_bound(a.begin(), a.end(), v) - a.begin()) - 1, 0, N - 2);
      const double span = std::max(1e-12, a[k[c] + 1] - a[k[c]]);
      t[c] = (v - a[k[c]]) / span;
      dt[c] = l[c] >= a.front() && l[c] <= a.back() ? 1. / span : 0.;
    }
    int o[3]{0, 1, 2};
    std::sort(o, o + 3, [&](int a, int b) { return t[a] > t[b]; });
    int d[3]{0, 0, 0};
    auto at = [&]() -> const std::array<double, 3>& {
      return shown[(std::size_t(k[2] + d[2]) * N + (k[1] + d[1])) * N + (k[0] + d[0])];
    };
    const double w[4]{1. - t[o[0]], t[o[0]] - t[o[1]], t[o[1]] - t[o[2]], t[o[2]]};
    std::array<double, 3> r{};
    const std::array<double, 3>* prev{};
    for(int v = 0; v < 4; v++)
    {
      if(v > 0)
        d[o[v - 1]] = 1;
      const auto& s = at();
      for(int c = 0; c < 3; c++)
        r[c] += w[v] * s[c];
      if(J && prev)
        for(int c = 0; c < 3; c++)
          (*J)(c, o[v - 1]) = (s[c] - (*prev)[c]) * dt[o[v - 1]];
      prev = &s;
    }
    return r;
  }

  //! l in [0, 1]^3 whose forward() is closest to e, by Gauss-Newton with the
  //! bounds as an active set.
  std::array<double, 3> closest(const std::array<double, 3>& e) const noexcept;

  //! The exact l with forward(l) = e, if e is shown at all.
  bool solve(const std::array<double, 3>& e, std::array<double, 3>& l) const;

  //! Axes finite and strictly increasing, samples finite, at most maxLevels
  //! codes per axis.
  bool wellFormed(int maxLevels = 17) const noexcept
  {
    if(!valid() || n() > maxLevels)
      return false;
    for(auto& a : axis)
    {
      if(!std::isfinite(a.front()))
        return false;
      for(std::size_t k = 1; k < a.size(); k++)
        if(!std::isfinite(a[k]) || !(a[k] > a[k - 1]))
          return false;
    }
    for(auto& v : shown)
      if(!std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2]))
        return false;
    return true;
  }
};

//! The tetrahedra of a ColourMixing with their barycentric inverses and a
//! uniform grid over the shown colours, for many solves.
class SCORE_PLUGIN_GFX_EXPORT MixingTetrahedra
{
public:
  explicit MixingTetrahedra(const ColourMixing& m);

  //! Among the tetrahedra holding e, the solution closest to hint, or the
  //! first one without a hint.
  bool solve(
      const std::array<double, 3>& e, std::array<double, 3>& l,
      const std::array<double, 3>* hint = nullptr) const noexcept
  {
    constexpr double eps = 1e-9;
    if(m_grid.empty())
      return false;
    const Eigen::Vector3d ev(e[0], e[1], e[2]);
    if(!ev.allFinite() || (ev.array() < m_lo.array()).any() || (ev.array() > m_hi.array()).any())
      return false;
    const auto& list
        = m_grid[(std::size_t(bin(2, e[2])) * m_bins + bin(1, e[1])) * m_bins + bin(0, e[0])];
    bool found = false;
    double bestDist = 0.;
    for(int k : list)
    {
      const auto& c = m_cells[k];
      if(((ev.array() < c.lo.array() - 1e-7) || (ev.array() > c.hi.array() + 1e-7)).any())
        continue;
      for(int t = c.first; t < c.first + c.count; t++)
      {
        const auto& tet = m_tets[t];
        const Eigen::Vector3d w = tet.inv * (ev - tet.s0);
        if(1. - w.sum() < -eps || w.minCoeff() < -eps)
          continue;
        const Eigen::Vector3d v = (tet.l0 + tet.dl * w).cwiseMax(0.).cwiseMin(1.);
        const std::array<double, 3> cand{v[0], v[1], v[2]};
        if(!hint)
        {
          l = cand;
          return true;
        }
        const double dist = (v - Eigen::Vector3d((*hint)[0], (*hint)[1], (*hint)[2])).squaredNorm();
        if(!found || dist < bestDist)
        {
          l = cand;
          bestDist = dist;
          found = true;
        }
      }
    }
    return found;
  }

private:
  int bin(int a, double v) const noexcept
  {
    const double t = (v - m_lo[a]) / (m_hi[a] - m_lo[a]) * m_bins;
    return t > 0. ? std::min(int(t), m_bins - 1) : 0;
  }

  struct Tet
  {
    Eigen::Matrix3d inv, dl;
    Eigen::Vector3d s0, l0;
  };
  struct Cell
  {
    Eigen::Vector3d lo, hi;
    int first{}, count{};
  };
  std::vector<Tet> m_tets;
  std::vector<Cell> m_cells;
  std::vector<std::vector<int>> m_grid;
  Eigen::Vector3d m_lo, m_hi;
  int m_bins{};
};

inline bool ColourMixing::solve(const std::array<double, 3>& e, std::array<double, 3>& l) const
{
  return MixingTetrahedra(*this).solve(e, l);
}

//! ColourMixing::solve on a K^3 grid of shown light e = D (j / (K - 1))^2 per
//! axis, red fastest. A colour just out of reach (within 3 %) takes the
//! closest reachable one; one further out is scaled towards black until it is
//! shown exactly, which keeps its hue when a darker colour of that hue is shown
//! at all, and takes the closest reachable one otherwise. Where several
//! branches of a non-injective mixing show e, each node takes the one nearest
//! its already solved neighbour.
struct SCORE_PLUGIN_GFX_EXPORT MixingInverse
{
  static constexpr int maxK = 65;
  int K{};
  double D{2.};
  std::vector<float> l;

  bool valid() const noexcept { return K >= 2 && l.size() == std::size_t(K) * K * K * 3; }

  //! D <= 0: just above the brightest shown component.
  static MixingInverse build(const ColourMixing& m, int K = 33, double D = 0.);

  std::array<double, 3> operator()(std::array<double, 3> e) const noexcept
  {
    for(auto& v : e)
      if(!std::isfinite(v))
        v = 0.;
    if(const double top = std::max({e[0], e[1], e[2]}); top > D)
      for(auto& v : e)
        v *= D / top;
    double p[3];
    int k[3];
    double t[3];
    for(int c = 0; c < 3; c++)
    {
      p[c] = std::sqrt(std::clamp(e[c] / D, 0., 1.)) * (K - 1);
      k[c] = std::min(int(p[c]), K - 2);
      t[c] = p[c] - k[c];
    }
    std::array<double, 3> r{};
    for(int dz = 0; dz < 2; dz++)
      for(int dy = 0; dy < 2; dy++)
        for(int dx = 0; dx < 2; dx++)
        {
          const double w = (dx ? t[0] : 1. - t[0]) * (dy ? t[1] : 1. - t[1])
                           * (dz ? t[2] : 1. - t[2]);
          const float* s
              = l.data() + ((std::size_t(k[2] + dz) * K + (k[1] + dy)) * K + (k[0] + dx)) * 3;
          for(int c = 0; c < 3; c++)
            r[c] += w * s[c];
        }
    return r;
  }
};

//! The largest s with s M (1, 1, 1) shown through the projector's mixing:
//! the table maps a colour past the gamut onto its edge, along its hue.
SCORE_PLUGIN_GFX_EXPORT double maxWhiteScale(
    const Eigen::Matrix3d& M, const ColourMixing& mix, const MixingInverse& inv) noexcept;

//! PSNR in dB of a against b (same size, values in [0, 1]).
inline double psnr(const std::vector<double>& a, const std::vector<double>& b) noexcept
{
  if(a.size() != b.size())
    return 0.;
  double se = 0.;
  for(std::size_t i = 0; i < a.size(); i++)
    se += (a[i] - b[i]) * (a[i] - b[i]);
  se /= std::max<std::size_t>(1, a.size());
  return se <= 0. ? 99. : 10. * std::log10(1. / se);
}

//! Linear sRGB -> CIE 1976 L*a*b* (D65).
SCORE_PLUGIN_GFX_EXPORT std::array<double, 3> linearSrgbToLab(const std::array<double, 3>& c) noexcept;

inline double deltaE76(const std::array<double, 3>& a, const std::array<double, 3>& b) noexcept
{
  const auto la = linearSrgbToLab(a), lb = linearSrgbToLab(b);
  return std::sqrt(
      (la[0] - lb[0]) * (la[0] - lb[0]) + (la[1] - lb[1]) * (la[1] - lb[1])
      + (la[2] - lb[2]) * (la[2] - lb[2]));
}
}
