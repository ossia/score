#pragma once
#include <Eigen/Dense>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Photometric calibration of a projector-camera pair, in the conventions of
// immersive-analysis/CONVENTIONS.md:
//  - Camera response: Debevec & Malik 1997, g(Z) = ln E + ln dt from pixels
//    seen at several exposures, least squares with a weighted second
//    derivative smoothness term and the hat weighting, g(Zmid) = 0.
//  - Projector response: per channel, normalised f(0) = 0, f(1) = 1, from a
//    ramp of uniform levels measured by the linearised camera (Majumder &
//    Stevens 2004; Brown, Majumder & Yang 2005), made monotonic by isotonic
//    regression and interpolated with a monotone cubic (Fritsch & Carlson 1980).
//  - Radiometric compensation: per pixel C = V P + F (Nayar et al. 2003), V
//    from the red, green and blue primaries, P = V^-1 (T c) with a global
//    target T chosen so a given fraction of the pixels can reach it.
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
    const auto& gc = g[c];
    const double z = std::clamp(v, 0., 1.) * (levels - 1);
    const int z0 = std::min(int(z), levels - 2);
    const double t = z - z0;
    const double gz = gc[z0] * (1. - t) + gc[z0 + 1] * t;
    return std::exp(gz - gc[levels - 1]);
  }
};

//! Debevec & Malik's hat: z - Zmin below the middle, Zmax - z above.
inline double hatWeight(int z, int levels) noexcept
{
  const int zmax = levels - 1;
  return z <= zmax / 2 ? double(z) : double(zmax - z);
}

//! One channel. Z[i * P + j]: code of sample i at exposure j, < 0 = missing.
//! Returns g (levels values, g(levels / 2) = 0), and the ln E of each sample.
inline std::vector<double> solveCameraResponse(
    const std::vector<int>& Z, int samples, const std::vector<double>& lnDt, int levels,
    double lambda, std::vector<double>* lnE = nullptr)
{
  const int P = int(lnDt.size());
  const int n = levels + samples;
  Eigen::MatrixXd AtA = Eigen::MatrixXd::Zero(n, n);
  Eigen::VectorXd Atb = Eigen::VectorXd::Zero(n);
  auto addRow = [&](const int* idx, const double* val, int k, double b) {
    for(int a = 0; a < k; a++)
    {
      Atb[idx[a]] += val[a] * b;
      for(int c = 0; c < k; c++)
        AtA(idx[a], idx[c]) += val[a] * val[c];
    }
  };
  for(int i = 0; i < samples; i++)
    for(int j = 0; j < P; j++)
    {
      const int z = Z[std::size_t(i) * P + j];
      if(z < 0 || z >= levels)
        continue;
      const double w = hatWeight(z, levels);
      if(w <= 0.)
        continue;
      const int idx[2]{z, levels + i};
      const double val[2]{w, -w};
      addRow(idx, val, 2, w * lnDt[j]);
    }
  {
    const int idx[1]{levels / 2};
    const double val[1]{1.};
    addRow(idx, val, 1, 0.);
  }
  for(int z = 1; z < levels - 1; z++)
  {
    const double w = lambda * hatWeight(z, levels);
    const int idx[3]{z - 1, z, z + 1};
    const double val[3]{w, -2. * w, w};
    addRow(idx, val, 3, 0.);
  }
  AtA.diagonal().array() += 1e-9;
  const Eigen::VectorXd x = AtA.ldlt().solve(Atb);
  std::vector<double> g(levels);
  for(int z = 0; z < levels; z++)
    g[z] = x[z];
  for(int z = 1; z < levels; z++)
    g[z] = std::max(g[z], g[z - 1] + 1e-6);
  if(lnE)
  {
    lnE->resize(samples);
    for(int i = 0; i < samples; i++)
      (*lnE)[i] = x[levels + i];
  }
  return g;
}

//! Pool adjacent violators: the non-decreasing least-squares fit of y.
inline std::vector<double> isotonic(const std::vector<double>& y, const std::vector<double>& w)
{
  struct Block
  {
    double sum, weight;
    int count;
  };
  std::vector<Block> b;
  for(std::size_t i = 0; i < y.size(); i++)
  {
    b.push_back({y[i] * w[i], w[i], 1});
    while(b.size() > 1
          && b[b.size() - 2].sum / b[b.size() - 2].weight > b.back().sum / b.back().weight)
    {
      auto last = b.back();
      b.pop_back();
      b.back().sum += last.sum;
      b.back().weight += last.weight;
      b.back().count += last.count;
    }
  }
  std::vector<double> out;
  for(auto& k : b)
    out.insert(out.end(), k.count, k.sum / k.weight);
  return out;
}

//! Monotone piecewise cubic Hermite interpolation through increasing (x, y).
class MonotoneCubic
{
public:
  MonotoneCubic() = default;
  MonotoneCubic(std::vector<double> x, std::vector<double> y)
      : m_x{std::move(x)}
      , m_y{std::move(y)}
  {
    const std::size_t n = m_x.size();
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
    if(x <= m_x.front())
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
inline ChannelResponse fitChannelResponse(const std::vector<double>& x, const std::vector<double>& m)
{
  ChannelResponse r;
  r.x.push_back(0.);
  r.measured.push_back(0.);
  const double top = m.empty() ? 1. : std::max(1e-12, m.back());
  for(std::size_t k = 0; k < x.size(); k++)
  {
    r.x.push_back(x[k]);
    r.measured.push_back(m[k] / top);
  }
  std::vector<double> w(r.x.size(), 1.);
  w.front() = w.back() = 1e6;
  r.fitted = isotonic(r.measured, w);
  r.fitted.front() = 0.;
  r.fitted.back() = 1.;
  for(std::size_t k = 1; k < r.fitted.size(); k++)
    r.fitted[k] = std::max(r.fitted[k], r.fitted[k - 1] + 1e-6 * (r.x[k] - r.x[k - 1]));
  const double end = r.fitted.back();
  for(auto& f : r.fitted)
    f /= end;
  r.curve = MonotoneCubic(r.x, r.fitted);
  double sxy = 0., sxx = 0.;
  for(std::size_t k = 1; k + 1 < r.x.size(); k++)
    if(r.measured[k] > 0.01 && r.x[k] < 1.)
    {
      const double lx = std::log(r.x[k]), ly = std::log(r.measured[k]);
      sxy += lx * ly;
      sxx += lx * lx;
    }
  if(sxx > 0.)
    r.gamma = sxy / sxx;
  return r;
}

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
inline bool compensationMatrix(
    const Eigen::Matrix3d& V, const std::array<double, 3>& k, Eigen::Matrix3d& M,
    double maxCondition = 1e3) noexcept
{
  Eigen::JacobiSVD<Eigen::Matrix3d> svd(V);
  const auto& s = svd.singularValues();
  if(!(s[0] > 0.) || s[2] <= 0. || s[0] / s[2] > maxCondition)
  {
    M.setIdentity();
    return false;
  }
  M = V.inverse() * Eigen::Vector3d(k[0], k[1], k[2]).asDiagonal();
  return M.allFinite();
}

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
struct ColourMixing
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

  std::array<double, 3> forward(const std::array<double, 3>& l) const noexcept
  {
    const int N = n();
    int k[3];
    double t[3];
    for(int c = 0; c < 3; c++)
    {
      const auto& a = axis[c];
      const double v = std::clamp(l[c], a.front(), a.back());
      k[c] = std::clamp(
          int(std::upper_bound(a.begin(), a.end(), v) - a.begin()) - 1, 0, N - 2);
      t[c] = (v - a[k[c]]) / std::max(1e-12, a[k[c] + 1] - a[k[c]]);
    }
    int o[3]{0, 1, 2};
    std::sort(o, o + 3, [&](int a, int b) { return t[a] > t[b]; });
    int d[3]{0, 0, 0};
    auto at = [&] {
      return shown[(std::size_t(k[2] + d[2]) * N + (k[1] + d[1])) * N + (k[0] + d[0])];
    };
    const double w[4]{1. - t[o[0]], t[o[0]] - t[o[1]], t[o[1]] - t[o[2]], t[o[2]]};
    std::array<double, 3> r{};
    for(int v = 0; v < 4; v++)
    {
      if(v > 0)
        d[o[v - 1]] = 1;
      const auto& s = at();
      for(int c = 0; c < 3; c++)
        r[c] += w[v] * s[c];
    }
    return r;
  }

  //! l in [0, 1]^3 whose forward() is closest to e, by projected Gauss-Newton.
  std::array<double, 3> closest(const std::array<double, 3>& e) const noexcept
  {
    std::array<double, 3> l;
    for(int c = 0; c < 3; c++)
      l[c] = std::clamp(e[c], 0., 1.);
    auto err = [&](const std::array<double, 3>& x, Eigen::Vector3d& f) {
      const auto y = forward(x);
      for(int c = 0; c < 3; c++)
        f[c] = y[c] - e[c];
      return f.squaredNorm();
    };
    Eigen::Vector3d f;
    double cost = err(l, f);
    for(int it = 0; it < 40 && cost > 1e-16; it++)
    {
      Eigen::Matrix3d J;
      for(int c = 0; c < 3; c++)
      {
        auto x = l;
        const double h = x[c] > 0.5 ? -1e-5 : 1e-5;
        x[c] += h;
        Eigen::Vector3d g;
        err(x, g);
        J.col(c) = (g - f) / h;
      }
      const Eigen::Vector3d step = J.colPivHouseholderQr().solve(-f);
      if(!step.allFinite())
        break;
      bool better = false;
      for(double a = 1.; a > 1e-3 && !better; a *= 0.5)
      {
        std::array<double, 3> x;
        for(int c = 0; c < 3; c++)
          x[c] = std::clamp(l[c] + a * step[c], 0., 1.);
        Eigen::Vector3d g;
        if(const double cg = err(x, g); cg < cost)
        {
          l = x;
          f = g;
          cost = cg;
          better = true;
        }
      }
      if(!better)
        break;
    }
    return l;
  }

  //! The exact l with forward(l) = e, if e is shown at all.
  bool solve(const std::array<double, 3>& e, std::array<double, 3>& l) const noexcept
  {
    const int N = n();
    static constexpr int perms[6][3]{{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    constexpr double eps = 1e-9;
    for(int z = 0; z + 1 < N; z++)
      for(int y = 0; y + 1 < N; y++)
        for(int x = 0; x + 1 < N; x++)
        {
          const int base[3]{x, y, z};
          auto value = [&](const int* d) -> const std::array<double, 3>& {
            return shown[(std::size_t(z + d[2]) * N + (y + d[1])) * N + (x + d[0])];
          };
          {
            std::array<double, 3> lo{1e30, 1e30, 1e30}, hi{-1e30, -1e30, -1e30};
            for(int c = 0; c < 8; c++)
            {
              const int d[3]{c & 1, (c >> 1) & 1, (c >> 2) & 1};
              const auto& v = value(d);
              for(int a = 0; a < 3; a++)
              {
                lo[a] = std::min(lo[a], v[a]);
                hi[a] = std::max(hi[a], v[a]);
              }
            }
            bool inside = true;
            for(int a = 0; a < 3; a++)
              inside &= e[a] >= lo[a] - 1e-7 && e[a] <= hi[a] + 1e-7;
            if(!inside)
              continue;
          }
          for(auto& o : perms)
          {
            int d[4][3]{};
            for(int v = 1; v < 4; v++)
            {
              std::copy(d[v - 1], d[v - 1] + 3, d[v]);
              d[v][o[v - 1]] = 1;
            }
            const auto& s0 = value(d[0]);
            Eigen::Matrix3d A;
            for(int v = 1; v < 4; v++)
            {
              const auto& sv = value(d[v]);
              for(int a = 0; a < 3; a++)
                A(a, v - 1) = sv[a] - s0[a];
            }
            Eigen::FullPivLU<Eigen::Matrix3d> lu(A);
            if(!lu.isInvertible())
              continue;
            const Eigen::Vector3d w
                = lu.solve(Eigen::Vector3d(e[0] - s0[0], e[1] - s0[1], e[2] - s0[2]));
            const double w0 = 1. - w.sum();
            if(w0 < -eps || w.minCoeff() < -eps)
              continue;
            for(int a = 0; a < 3; a++)
            {
              const auto& ax = axis[a];
              double v = 0.;
              for(int k = 0; k < 4; k++)
              {
                const double wk = k == 0 ? w0 : w[k - 1];
                v += wk * ax[base[a] + d[k][a]];
              }
              l[a] = std::clamp(v, 0., 1.);
            }
            return true;
          }
        }
    return false;
  }
};

//! ColourMixing::solve on a K^3 grid of shown light e = D (j / (K - 1))^2 per
//! axis, red fastest. A colour just out of reach takes the closest reachable
//! one; one further out is scaled towards black until reachable, which keeps
//! its hue.
struct MixingInverse
{
  int K{};
  double D{2.};
  std::vector<float> l;

  bool valid() const noexcept { return K >= 2 && l.size() == std::size_t(K) * K * K * 3; }

  //! D <= 0: just above the brightest shown component.
  static MixingInverse build(const ColourMixing& m, int K = 33, double D = 0.)
  {
    MixingInverse r;
    if(!m.valid())
      return r;
    if(D <= 0.)
    {
      for(auto& v : m.shown)
        D = std::max({D, v[0], v[1], v[2]});
      D = std::max(1e-3, 1.02 * D);
    }
    r.K = K;
    r.D = D;
    r.l.resize(std::size_t(K) * K * K * 3);
    auto node = [&](int j) { return D * double(j) * j / double((K - 1) * (K - 1)); };
    auto fit = [&](const std::array<double, 3>& e, std::array<double, 3>& l) {
      if(m.solve(e, l))
        return true;
      l = m.closest(e);
      const auto y = m.forward(l);
      double miss = 0., norm = 0.;
      for(int c = 0; c < 3; c++)
      {
        miss += (y[c] - e[c]) * (y[c] - e[c]);
        norm += e[c] * e[c];
      }
      return std::sqrt(miss) <= 0.03 * std::sqrt(norm) + 0.005 * D;
    };
    for(int b = 0; b < K; b++)
      for(int g = 0; g < K; g++)
        for(int x = 0; x < K; x++)
        {
          const std::array<double, 3> e{node(x), node(g), node(b)};
          std::array<double, 3> l{};
          if(!fit(e, l))
          {
            double lo = 0., hi = 1.;
            std::array<double, 3> best = l;
            for(int it = 0; it < 20; it++)
            {
              const double k = 0.5 * (lo + hi);
              std::array<double, 3> lk;
              if(fit({k * e[0], k * e[1], k * e[2]}, lk))
              {
                lo = k;
                best = lk;
              }
              else
                hi = k;
            }
            l = best;
          }
          float* o = r.l.data() + ((std::size_t(b) * K + g) * K + x) * 3;
          for(int c = 0; c < 3; c++)
            o[c] = float(l[c]);
        }
    return r;
  }

  std::array<double, 3> operator()(std::array<double, 3> e) const noexcept
  {
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
inline double maxWhiteScale(
    const Eigen::Matrix3d& M, const ColourMixing& mix, const MixingInverse& inv) noexcept
{
  if(!mix.valid() || !inv.valid())
    return maxWhiteScale(M);
  const Eigen::Vector3d v = M * Eigen::Vector3d::Ones();
  const double top = v.maxCoeff();
  if(!(top > 0.))
    return 0.;
  const double far = inv.D / top;
  const std::array<double, 3> e{far * v[0], far * v[1], far * v[2]};
  const auto y = mix.forward(inv(e));
  const double k = (y[0] * e[0] + y[1] * e[1] + y[2] * e[2])
                   / (e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
  return far * std::clamp(k, 0., 1.);
}

//! PSNR in dB of a against b (same size, values in [0, 1]).
inline double psnr(const std::vector<double>& a, const std::vector<double>& b) noexcept
{
  double se = 0.;
  for(std::size_t i = 0; i < a.size(); i++)
    se += (a[i] - b[i]) * (a[i] - b[i]);
  se /= std::max<std::size_t>(1, a.size());
  return se <= 0. ? 99. : 10. * std::log10(1. / se);
}

//! Linear sRGB -> CIE 1976 L*a*b* (D65).
inline std::array<double, 3> linearSrgbToLab(const std::array<double, 3>& c) noexcept
{
  const double X = 0.4124 * c[0] + 0.3576 * c[1] + 0.1805 * c[2];
  const double Y = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
  const double Z = 0.0193 * c[0] + 0.1192 * c[1] + 0.9505 * c[2];
  auto f = [](double t) {
    return t > 216. / 24389. ? std::cbrt(t) : (24389. / 27. * t + 16.) / 116.;
  };
  const double fx = f(X / 0.95047), fy = f(Y), fz = f(Z / 1.08883);
  return {116. * fy - 16., 500. * (fx - fy), 200. * (fy - fz)};
}

inline double deltaE76(const std::array<double, 3>& a, const std::array<double, 3>& b) noexcept
{
  const auto la = linearSrgbToLab(a), lb = linearSrgbToLab(b);
  return std::sqrt(
      (la[0] - lb[0]) * (la[0] - lb[0]) + (la[1] - lb[1]) * (la[1] - lb[1])
      + (la[2] - lb[2]) * (la[2] - lb[2]));
}
}
