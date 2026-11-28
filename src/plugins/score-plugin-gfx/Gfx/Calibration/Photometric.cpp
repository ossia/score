#include <Gfx/Calibration/Photometric.hpp>
#include <Eigen/Dense>

namespace score::gfx::photometric
{

bool plausibleResponse(
    const CameraResponse& r, double minSpan, double tolerance) noexcept
{
  if(!r.valid())
    return false;
  const int lo = r.levels / 8;
  const int hi = r.levels - 1 - r.levels / 8;
  for(int c = 0; c < 3; c++)
  {
    const auto& g = r.g[c];
    if(!(g[hi] - g[lo] >= minSpan))
      return false;
    double top = g[lo];
    for(int z = lo + 1; z <= hi; z++)
    {
      top = std::max(top, g[z]);
      if(g[z] < top - tolerance)
        return false;
    }
  }
  return true;
}

std::vector<double> solveCameraResponse(
    const std::vector<int>& Z, int samples, const std::vector<double>& lnDt, int levels,
    double lambda, std::vector<double>* lnE)
{
  const int P = int(lnDt.size());
  if(levels < 2 || samples < 0 || Z.size() < std::size_t(samples) * P)
    return {};
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
      if(z < 0 || z >= levels || !std::isfinite(lnDt[j]))
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
  if(!x.allFinite())
    return {};
  std::vector<double> g(levels);
  for(int z = 0; z < levels; z++)
    g[z] = x[z];
  for(int z = 1; z < levels; z++)
    g[z] = std::max(g[z], g[z - 1] + 1e-6);
  const double mid = g[levels / 2];
  for(double& v : g)
    v -= mid;
  if(lnE)
  {
    lnE->resize(samples);
    for(int i = 0; i < samples; i++)
      (*lnE)[i] = x[levels + i] - mid;
  }
  return g;
}

std::vector<double> isotonic(const std::vector<double>& y, const std::vector<double>& w)
{
  if(w.size() != y.size())
    return y;
  struct Block
  {
    double sum, weight;
    int count;
  };
  std::vector<Block> b;
  for(std::size_t i = 0; i < y.size(); i++)
  {
    const double wi = w[i] > 1e-12 ? w[i] : 1e-12;
    b.push_back({y[i] * wi, wi, 1});
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

ChannelResponse fitChannelResponse(const std::vector<double>& x, const std::vector<double>& m)
{
  ChannelResponse r;
  const bool usable = !x.empty() && m.size() == x.size() && m.back() > 0.
                      && std::isfinite(m.back())
                      && std::all_of(x.begin(), x.end(), [](double v) { return v > 0. && v <= 1.; })
                      && std::is_sorted(x.begin(), x.end())
                      && std::all_of(m.begin(), m.end(), [](double v) { return std::isfinite(v); });
  if(!usable)
  {
    r.x = r.measured = r.fitted = {0., 1.};
    r.gamma = 1.;
    r.curve = MonotoneCubic(r.x, r.fitted);
    return r;
  }
  r.valid = true;
  r.x.push_back(0.);
  r.measured.push_back(0.);
  const double top = m.back();
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

bool compensationMatrix(
    const Eigen::Matrix3d& V, const std::array<double, 3>& k, Eigen::Matrix3d& M,
    double maxCondition) noexcept
{
  Eigen::JacobiSVD<Eigen::Matrix3d> svd(V);
  const auto& s = svd.singularValues();
  if(!(s[0] > 0.) || s[2] <= 0. || s[0] / s[2] > maxCondition)
  {
    M.setIdentity();
    return false;
  }
  M = V.inverse() * Eigen::Vector3d(k[0], k[1], k[2]).asDiagonal();
  if(!M.allFinite())
  {
    M.setIdentity();
    return false;
  }
  return true;
}

double maxWhiteScale(
    const Eigen::Matrix3d& M, const ColourMixing& mix, const MixingInverse& inv) noexcept
{
  if(!mix.valid() || !inv.valid())
    return maxWhiteScale(M);
  const Eigen::Vector3d v = M * Eigen::Vector3d::Ones();
  const double top = v.maxCoeff();
  if(!(top > 0.))
    return 0.;
  const double reach = inv.D / top;
  const std::array<double, 3> e{reach * v[0], reach * v[1], reach * v[2]};
  const auto y = mix.forward(inv(e));
  const double k = (y[0] * e[0] + y[1] * e[1] + y[2] * e[2])
                   / (e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
  return reach * std::clamp(k, 0., 1.);
}

std::array<double, 3> linearSrgbToLab(const std::array<double, 3>& c) noexcept
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

std::array<double, 3> ColourMixing::closest(const std::array<double, 3>& e) const noexcept
{
  std::array<double, 3> l;
  for(int c = 0; c < 3; c++)
    l[c] = e[c] > 0. ? std::min(e[c], 1.) : 0.;
  auto err = [&](const std::array<double, 3>& x, Eigen::Vector3d& f, Eigen::Matrix3d& J) {
    const auto y = forward(x, &J);
    for(int c = 0; c < 3; c++)
      f[c] = y[c] - e[c];
    return f.squaredNorm();
  };
  Eigen::Vector3d f;
  Eigen::Matrix3d J;
  double cost = err(l, f, J);
  for(int it = 0; it < 40 && cost > 1e-16; it++)
  {
    Eigen::Matrix3d Ja = J;
    bool active[3]{};
    Eigen::Vector3d step;
    for(int pass = 0; pass < 3; pass++)
    {
      step = Ja.colPivHouseholderQr().solve(-f);
      for(int c = 0; c < 3; c++)
        if(active[c])
          step[c] = 0.;
      bool clipped = false;
      for(int c = 0; c < 3; c++)
        if(!active[c] && ((l[c] <= 0. && step[c] < 0.) || (l[c] >= 1. && step[c] > 0.)))
        {
          active[c] = clipped = true;
          Ja.col(c).setZero();
        }
      if(!clipped)
        break;
    }
    if(!step.allFinite())
      break;
    bool better = false;
    for(double a = 1.; a > 1e-3 && !better; a *= 0.5)
    {
      std::array<double, 3> x;
      for(int c = 0; c < 3; c++)
        x[c] = std::clamp(l[c] + a * step[c], 0., 1.);
      Eigen::Vector3d g;
      Eigen::Matrix3d Jx;
      if(const double cg = err(x, g, Jx); cg < cost)
      {
        l = x;
        f = g;
        J = Jx;
        cost = cg;
        better = true;
      }
    }
    if(!better)
      break;
  }
  return l;
}

MixingTetrahedra::MixingTetrahedra(const ColourMixing& m)
{
  const int N = m.n();
  if(!m.valid())
    return;
  static constexpr int perms[6][3]{{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  m_lo.setConstant(1e30);
  m_hi.setConstant(-1e30);
  for(int z = 0; z + 1 < N; z++)
    for(int y = 0; y + 1 < N; y++)
      for(int x = 0; x + 1 < N; x++)
      {
        auto value = [&](const int* d) {
          const auto& v = m.shown[(std::size_t(z + d[2]) * N + (y + d[1])) * N + (x + d[0])];
          return Eigen::Vector3d(v[0], v[1], v[2]);
        };
        auto code = [&](const int* d) {
          return Eigen::Vector3d(
              m.axis[0][x + d[0]], m.axis[1][y + d[1]], m.axis[2][z + d[2]]);
        };
        Cell cell;
        cell.lo.setConstant(1e30);
        cell.hi.setConstant(-1e30);
        for(int c = 0; c < 8; c++)
        {
          const int d[3]{c & 1, (c >> 1) & 1, (c >> 2) & 1};
          const Eigen::Vector3d v = value(d);
          cell.lo = cell.lo.cwiseMin(v);
          cell.hi = cell.hi.cwiseMax(v);
        }
        cell.first = int(m_tets.size());
        for(auto& o : perms)
        {
          int d[4][3]{};
          for(int v = 1; v < 4; v++)
          {
            std::copy(d[v - 1], d[v - 1] + 3, d[v]);
            d[v][o[v - 1]] = 1;
          }
          Tet t;
          t.s0 = value(d[0]);
          t.l0 = code(d[0]);
          Eigen::Matrix3d A;
          for(int v = 1; v < 4; v++)
          {
            A.col(v - 1) = value(d[v]) - t.s0;
            t.dl.col(v - 1) = code(d[v]) - t.l0;
          }
          Eigen::FullPivLU<Eigen::Matrix3d> lu(A);
          if(!lu.isInvertible())
            continue;
          t.inv = lu.inverse();
          m_tets.push_back(t);
        }
        cell.count = int(m_tets.size()) - cell.first;
        m_lo = m_lo.cwiseMin(cell.lo);
        m_hi = m_hi.cwiseMax(cell.hi);
        m_cells.push_back(cell);
      }
  if(m_cells.empty())
    return;
  m_bins = std::clamp(N - 1, 1, 16);
  m_lo.array() -= 1e-6;
  m_hi.array() += 1e-6;
  m_grid.resize(std::size_t(m_bins) * m_bins * m_bins);
  for(int k = 0; k < int(m_cells.size()); k++)
  {
    const auto& c = m_cells[k];
    int b0[3], b1[3];
    for(int a = 0; a < 3; a++)
    {
      b0[a] = bin(a, c.lo[a] - 1e-7);
      b1[a] = bin(a, c.hi[a] + 1e-7);
    }
    for(int z = b0[2]; z <= b1[2]; z++)
      for(int y = b0[1]; y <= b1[1]; y++)
        for(int x = b0[0]; x <= b1[0]; x++)
          m_grid[(std::size_t(z) * m_bins + y) * m_bins + x].push_back(k);
  }
}

MixingInverse MixingInverse::build(const ColourMixing& m, int K, double D)
{
  MixingInverse r;
  if(!m.wellFormed() || K < 2 || K > maxK || !std::isfinite(D))
    return r;
  if(D <= 0.)
  {
    for(auto& v : m.shown)
      D = std::max({D, v[0], v[1], v[2]});
    D = std::max(1e-3, 1.02 * D);
  }
  const MixingTetrahedra tets{m};
  r.K = K;
  r.D = D;
  r.l.resize(std::size_t(K) * K * K * 3);
  auto node = [&](int j) { return D * double(j) * j / double((K - 1) * (K - 1)); };
  auto fit = [&](const std::array<double, 3>& e, std::array<double, 3>& l,
                 const std::array<double, 3>* hint) {
    if(tets.solve(e, l, hint))
      return true;
    l = m.closest(e);
    const auto y = m.forward(l);
    double miss = 0., norm = 0.;
    for(int c = 0; c < 3; c++)
    {
      miss += (y[c] - e[c]) * (y[c] - e[c]);
      norm += e[c] * e[c];
    }
    return std::sqrt(miss) <= 0.03 * std::sqrt(norm) + 1e-4 * D;
  };
  for(int b = 0; b < K; b++)
    for(int g = 0; g < K; g++)
      for(int x = 0; x < K; x++)
      {
        const std::size_t idx = (std::size_t(b) * K + g) * K + x;
        std::array<double, 3> prev;
        const std::array<double, 3>* hint = nullptr;
        if(const std::size_t back = x > 0 ? 1 : g > 0 ? K : b > 0 ? std::size_t(K) * K : 0)
        {
          const float* p = r.l.data() + (idx - back) * 3;
          prev = {p[0], p[1], p[2]};
          hint = &prev;
        }
        const std::array<double, 3> e{node(x), node(g), node(b)};
        std::array<double, 3> l{};
        if(!fit(e, l, hint))
        {
          double lo = 0., hi = 1.;
          std::array<double, 3> best = l;
          for(int it = 0; it < 20; it++)
          {
            const double k = 0.5 * (lo + hi);
            std::array<double, 3> lk;
            if(tets.solve({k * e[0], k * e[1], k * e[2]}, lk, hint))
            {
              lo = k;
              best = lk;
            }
            else
              hi = k;
          }
          l = best;
        }
        float* o = r.l.data() + idx * 3;
        for(int c = 0; c < 3; c++)
          o[c] = float(l[c]);
      }
  return r;
}
}
