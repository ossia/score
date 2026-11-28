#include <Gfx/Calibration/Blending.hpp>
#include <Eigen/Dense>

namespace score::gfx::blend
{
namespace detail
{

void edt1d(const float* f, int n, float* d, int* v, float* z)
{
  int k = 0;
  v[0] = 0;
  z[0] = -std::numeric_limits<float>::infinity();
  z[1] = std::numeric_limits<float>::infinity();
  for(int q = 1; q < n; q++)
  {
    float s;
    while(true)
    {
      const int p = v[k];
      s = ((f[q] + float(q) * q) - (f[p] + float(p) * p)) / (2.f * q - 2.f * p);
      if(s <= z[k] && k > 0)
        k--;
      else
        break;
    }
    k++;
    v[k] = q;
    z[k] = s;
    z[k + 1] = std::numeric_limits<float>::infinity();
  }
  k = 0;
  for(int q = 0; q < n; q++)
  {
    while(z[k + 1] < q)
      k++;
    const float dq = float(q - v[k]);
    d[q] = dq * dq + f[v[k]];
  }
}
}

float rampWeight(float dist, float rampWidth, float exponent) noexcept
{
  if(dist < 0.f)
    return 0.f;
  constexpr float kFloor = 1e-3f;
  if(dist == 0.f)
    return kFloor;
  float s = rampWidth > 0.f ? std::min(1.f, dist / rampWidth) : dist;
  if(exponent != 1.f)
    s = std::pow(s, exponent);
  return s + kFloor;
}

float sampleBilinear(
    const std::vector<float>& m, int W, int H, double u, double v, int C,
    int c) noexcept
{
  if(W <= 0 || H <= 0 || !std::isfinite(u) || !std::isfinite(v))
    return 0.f;
  const double x = std::clamp(u, 0., double(W - 1)), y = std::clamp(v, 0., double(H - 1));
  const int x0 = std::min(int(x), std::max(0, W - 2)), y0 = std::min(int(y), std::max(0, H - 2));
  const int x1 = std::min(x0 + 1, W - 1), y1 = std::min(y0 + 1, H - 1);
  const float fx = float(x - x0), fy = float(y - y0);
  auto at = [&](int x, int y) { return m[(std::size_t(y) * W + x) * C + c]; };
  const float a = at(x0, y0), b = at(x1, y0);
  const float e = at(x0, y1), d = at(x1, y1);
  return (a * (1.f - fx) + b * fx) * (1.f - fy) + (e * (1.f - fx) + d * fx) * fy;
}

bool lights(
    const Projector& pj, const Reprojector& rj, const Vec3& X, const Options& opt, Vec2& px)
{
  if(!rj.find(X, px))
    return false;
  if(px.x() < -0.5 || px.y() < -0.5 || px.x() > pj.width - 0.5 || px.y() > pj.height - 0.5)
    return false;
  const int u = std::clamp(int(std::lround(px.x())), 0, pj.width - 1);
  const int v = std::clamp(int(std::lround(px.y())), 0, pj.height - 1);
  const int i = v * pj.width + u;
  if(!pj.valid(i))
    return false;
  double foot = 0.;
  if(pj.hasModel)
    foot = (X - pj.model.position()).norm() / std::max(1e-9, std::abs(pj.model.fx));
  else
  {
    const int un = u + 1 < pj.width ? u + 1 : u - 1;
    if(un >= 0 && pj.valid(v * pj.width + un))
      foot = (pj.point(v * pj.width + un) - pj.point(i)).norm();
  }
  const double tol = opt.visibilityTolerance + opt.footprintPixels * foot;
  Vec3 Xj;
  if(!rj.at(px.x(), px.y(), Xj))
    Xj = pj.point(i);
  return (Xj - X).norm() <= tol;
}

std::vector<uint8_t> innerEdges(
    const std::vector<Projector>& P, const std::vector<const Reprojector*>& rep, int i,
    const Options& opt)
{
  const auto& p = P[i];
  const int W = p.width, H = p.height;
  std::vector<uint8_t> site(std::size_t(W) * H, 0);
  constexpr int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const int k = y * W + x;
      if(!p.valid(k))
        continue;
      for(int d = 0; d < 4 && !site[k]; d++)
      {
        const int ox = x + dx[d], oy = y + dy[d];
        if(ox >= 0 && oy >= 0 && ox < W && oy < H && p.valid(oy * W + ox))
          continue;
        const int ix = x - dx[d], iy = y - dy[d];
        Vec3 out = p.point(k);
        if(ix >= 0 && iy >= 0 && ix < W && iy < H && p.valid(iy * W + ix))
          out += 2. * (p.point(k) - p.point(iy * W + ix));
        for(std::size_t j = 0; j < P.size() && !site[k]; j++)
        {
          Vec2 px;
          if(int(j) != i && lights(P[j], *rep[j], out, opt, px))
            site[k] = 1;
        }
      }
    }
  return site;
}
namespace detail
{

void constrainSlope(
    std::vector<std::vector<float>>& f, const std::vector<Projector>& P,
    const std::vector<std::vector<Link>>& links, double slope, const Options& opt,
    bool upper)
{
  const std::size_t N = P.size();
  auto dist = [&](const Projector& p, int a, int b) {
    const Vec3 Xa = p.point(a), Xb = p.point(b);
    const double d = (Xa - Xb).norm();
    if(!opt.hasViewer)
      return d;
    return d / std::max(1e-6, (0.5 * (Xa + Xb) - opt.viewer).norm());
  };
  std::vector<std::size_t> base(N + 1, 0);
  for(std::size_t i = 0; i < N; i++)
    base[i + 1] = base[i] + f[i].size();
  const std::size_t total = base[N];
  const float sign = upper ? -1.f : 1.f;
  std::vector<float> h(total);
  using Item = std::pair<float, std::size_t>;
  std::vector<Item> seeds;
  for(std::size_t i = 0; i < N; i++)
    for(std::size_t k = 0; k < f[i].size(); k++)
    {
      const float v = f[i][k];
      h[base[i] + k] = sign * (v > 0.f ? std::log(v) : -30.f);
      if(P[i].valid(int(k)))
        seeds.push_back({h[base[i] + k], base[i] + k});
    }

  std::vector<std::size_t> linkStart(total + 1, 0), linkTo;
  for(std::size_t i = 0; i < N; i++)
    for(const auto& l : links[i])
      if(l.projector < N && l.other < f[l.projector].size() && l.pixel < f[i].size())
        linkStart[base[l.projector] + l.other + 1]++;
  for(std::size_t k = 0; k < total; k++)
    linkStart[k + 1] += linkStart[k];
  linkTo.resize(linkStart[total]);
  {
    std::vector<std::size_t> fill(linkStart.begin(), linkStart.end() - 1);
    for(std::size_t i = 0; i < N; i++)
      for(const auto& l : links[i])
        if(l.projector < N && l.other < f[l.projector].size() && l.pixel < f[i].size())
          linkTo[fill[base[l.projector] + l.other]++] = base[i] + l.pixel;
  }

  std::priority_queue<Item, std::vector<Item>, std::greater<>> queue(
      std::greater<>{}, std::move(seeds));
  std::vector<uint8_t> done(total, 0);
  while(!queue.empty())
  {
    const auto [d, id] = queue.top();
    queue.pop();
    if(done[id] || d > h[id])
      continue;
    done[id] = 1;
    auto push = [&](std::size_t to, float v) {
      if(!done[to] && v < h[to])
      {
        h[to] = v;
        queue.push({v, to});
      }
    };
    const std::size_t pi
        = std::size_t(std::upper_bound(base.begin(), base.end(), id) - base.begin()) - 1;
    const Projector& p = P[pi];
    const int a = int(id - base[pi]);
    const int W = p.width, H = p.height, x = a % W, y = a / W;
    for(int dy = -1; dy <= 1; dy++)
      for(int dx = -1; dx <= 1; dx++)
      {
        const int nx = x + dx, ny = y + dy;
        if((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= W || ny >= H)
          continue;
        const int b = ny * W + nx;
        if(p.valid(b))
          push(base[pi] + b, d + float(slope * dist(p, a, b)));
      }
    for(std::size_t e = linkStart[id]; e < linkStart[id + 1]; e++)
      push(linkTo[e], d);
  }
  for(std::size_t i = 0; i < N; i++)
    for(std::size_t k = 0; k < f[i].size(); k++)
      f[i][k] = std::exp(sign * h[base[i] + k]);
}
}

Result compute(std::vector<Projector>& P, const Options& opt)
{
  Result r;
  const std::size_t N = P.size();
  r.alpha.resize(N);
  r.beta.resize(N);
  r.overlap.assign(N, 0.f);
  r.colorGain.assign(N, {1.f, 1.f, 1.f});
  if(N == 0)
  {
    r.status = "No projector";
    return r;
  }

  for(const auto& p : P)
    if(p.width <= 0 || p.height <= 0 || p.xyz.size() != std::size_t(p.width) * p.height * 4)
    {
      for(std::size_t i = 0; i < N; i++)
      {
        const std::size_t n = std::size_t(std::max(0, P[i].width)) * std::max(0, P[i].height);
        r.alpha[i].assign(n, 0.f);
        r.beta[i].assign(n, 0.f);
      }
      r.status = "Failed: an XYZ map does not match its size";
      return r;
    }

  bool rgbAll = opt.photometric && opt.perChannel;
  for(auto& p : P)
    rgbAll &= p.lum.size() == std::size_t(p.width) * p.height * 3;
  const int C = rgbAll ? 3 : 1;
  r.channels = C;

  std::vector<std::vector<float>> weight(N), lum(N), black(N);
  std::vector<std::unique_ptr<Reprojector>> rep(N);
  detail::parallelFor(int(N), opt.threads, [&](int i) {
    auto& p = P[i];
    const std::size_t n = std::size_t(p.width) * p.height;
    lum[i].assign(n * C, 1.f);
    black[i].assign(n * C, std::max(0.f, opt.blackLevel));
    const bool hasLum = opt.photometric && p.lum.size() == n * 3;
    const bool hasBlack = opt.photometric && p.black.size() == n * 3;
    for(std::size_t k = 0; k < n; k++)
      for(int c = 0; c < C; c++)
      {
        float& l = lum[i][k * C + c];
        if(hasLum)
          l = std::max(1e-6f, C == 1 ? detail::luma(p.lum.data() + k * 3) : p.lum[k * 3 + c]);
        if(hasBlack)
          black[i][k * C + c] = std::max(
              0.f, C == 1 ? detail::luma(p.black.data() + k * 3) : p.black[k * 3 + c]);
        else if(hasLum)
          black[i][k * C + c] = std::max(0.f, opt.blackLevel) * l;
      }
    rep[i] = std::make_unique<Reprojector>(p);
  });
  std::vector<const Reprojector*> reps;
  for(auto& rp : rep)
    reps.push_back(rp.get());
  detail::parallelFor(int(N), opt.threads, [&](int i) {
    const auto& p = P[i];
    const auto sites = innerEdges(P, reps, i, opt);
    const auto d = distanceToEdge(p, &sites);
    weight[i].resize(d.size());
    for(std::size_t k = 0; k < d.size(); k++)
      weight[i][k] = rampWeight(d[k], p.rampWidth, p.exponent);
  });

  // Colour: the white points of all projectors scaled to their common one.
  std::vector<std::array<double, 3>> chroma(N, {0., 0., 0.});
  std::vector<bool> hasRgb(N, false);
  for(std::size_t i = 0; i < N; i++)
  {
    const auto& p = P[i];
    const std::size_t n = std::size_t(p.width) * p.height;
    if(!opt.photometric || p.lum.size() != n * 3)
      continue;
    std::array<std::vector<float>, 3> c;
    for(std::size_t k = 0; k < n; k += 7)
    {
      if(!p.valid(int(k)))
        continue;
      const float y = detail::luma(p.lum.data() + k * 3);
      if(y <= 1e-6f)
        continue;
      for(int ch = 0; ch < 3; ch++)
        c[ch].push_back(p.lum[k * 3 + ch] / y);
    }
    if(c[0].size() < 16)
      continue;
    for(int ch = 0; ch < 3; ch++)
    {
      std::nth_element(c[ch].begin(), c[ch].begin() + c[ch].size() / 2, c[ch].end());
      chroma[i][ch] = c[ch][c[ch].size() / 2];
    }
    hasRgb[i] = true;
  }
  std::array<double, 3> common{1e30, 1e30, 1e30};
  int rgbCount = 0;
  for(std::size_t i = 0; i < N; i++)
    if(hasRgb[i])
    {
      rgbCount++;
      for(int ch = 0; ch < 3; ch++)
        common[ch] = std::min(common[ch], chroma[i][ch]);
    }
  std::array<float, 3> kc{1.f, 1.f, 1.f};
  if(C == 3)
  {
    if(rgbCount > 0)
    {
      const double y = 0.2126 * common[0] + 0.7152 * common[1] + 0.0722 * common[2];
      for(int ch = 0; ch < 3; ch++)
        kc[ch] = float(common[ch] / std::max(1e-9, y));
    }
    r.whiteChroma = kc;
  }
  else if(rgbCount >= 2)
    for(std::size_t i = 0; i < N; i++)
      if(hasRgb[i])
      {
        std::array<double, 3> g;
        for(int ch = 0; ch < 3; ch++)
          g[ch] = common[ch] / std::max(1e-9, chroma[i][ch]);
        const double gm = std::max({g[0], g[1], g[2]});
        for(int ch = 0; ch < 3; ch++)
          r.colorGain[i][ch] = float(g[ch] / gm);
        const float y = float(
            0.2126 * r.colorGain[i][0] * chroma[i][0] + 0.7152 * r.colorGain[i][1] * chroma[i][1]
            + 0.0722 * r.colorGain[i][2] * chroma[i][2]);
        for(float& l : lum[i])
          l *= y;
      }

  // Per pixel: partition b, blended white Lb, total black B (C per pixel).
  std::vector<std::vector<float>> part(N), Lb(N), B(N);
  std::vector<std::vector<detail::Link>> links(N);
  detail::parallelFor(int(N), opt.threads, [&](int i) {
    const auto& p = P[i];
    const std::size_t n = std::size_t(p.width) * p.height;
    part[i].assign(n, 0.f);
    Lb[i].assign(n * C, 0.f);
    B[i].assign(n * C, 0.f);
    std::size_t lit = 0, shared = 0;
    for(int k = 0; k < int(n); k++)
    {
      if(!p.valid(k))
        continue;
      lit++;
      const Vec3 X = p.point(k);
      double sw = weight[i][k];
      double swl[3], sb[3];
      for(int c = 0; c < C; c++)
      {
        swl[c] = weight[i][k] * lum[i][k * C + c];
        sb[c] = black[i][k * C + c];
      }
      bool any = false;
      for(std::size_t j = 0; j < N; j++)
      {
        if(j == std::size_t(i))
          continue;
        Vec2 px;
        if(!lights(P[j], *rep[j], X, opt, px))
          continue;
        any = true;
        const auto& pj = P[j];
        const float wj = sampleBilinear(weight[j], pj.width, pj.height, px.x(), px.y());
        sw += wj;
        for(int c = 0; c < C; c++)
        {
          swl[c] += wj * sampleBilinear(lum[j], pj.width, pj.height, px.x(), px.y(), C, c);
          sb[c] += sampleBilinear(black[j], pj.width, pj.height, px.x(), px.y(), C, c);
        }
        const int u = std::clamp(int(std::lround(px.x())), 0, pj.width - 1);
        const int v = std::clamp(int(std::lround(px.y())), 0, pj.height - 1);
        links[i].push_back({uint32_t(k), uint32_t(v * pj.width + u), uint32_t(j)});
      }
      shared += any;
      if(sw <= 0.)
      {
        sw = 1.;
        for(int c = 0; c < C; c++)
          swl[c] = lum[i][k * C + c];
      }
      part[i][k] = float(weight[i][k] / sw);
      for(int c = 0; c < C; c++)
      {
        Lb[i][k * C + c] = float(swl[c] / sw);
        B[i][k * C + c] = float(sb[c]);
      }
    }
    r.overlap[i] = lit ? float(shared) / float(lit) : 0.f;
  });

  auto channel = [&](const std::vector<std::vector<float>>& f, int c) {
    std::vector<std::vector<float>> out(N);
    for(std::size_t i = 0; i < N; i++)
    {
      out[i].resize(f[i].size() / C);
      for(std::size_t k = 0; k < out[i].size(); k++)
        out[i][k] = f[i][k * C + c];
    }
    return out;
  };

  // Targets: white W' <= B + Lb, black B' >= B, both with a bounded slope.
  // Per channel, the white is the common chromaticity times a luminance Y'
  // under the channel that limits it.
  std::vector<std::vector<float>> Y(N), Bt(N);
  for(std::size_t i = 0; i < N; i++)
  {
    const std::size_t n = part[i].size();
    Y[i].resize(n);
    for(std::size_t k = 0; k < n; k++)
    {
      float y = std::numeric_limits<float>::infinity();
      for(int c = 0; c < C; c++)
        y = std::min(y, (B[i][k * C + c] + Lb[i][k * C + c]) / kc[c]);
      Y[i][k] = y;
    }
  }
  if(opt.luminanceUniformity)
    detail::constrainSlope(Y, P, links, std::max(0., opt.maxLogSlope), opt, false);
  for(std::size_t i = 0; i < N; i++)
    Bt[i] = B[i];
  if(opt.blackCompensation)
  {
    bool anyBlack = false;
    for(auto& b : B)
      for(float v : b)
        anyBlack |= v > 0.f;
    if(anyBlack)
      for(int c = 0; c < C; c++)
      {
        auto bc = channel(B, c);
        detail::constrainSlope(
            bc, P, links, opt.luminanceUniformity ? std::max(0., opt.maxLogSlope) : 0., opt,
            true);
        for(std::size_t i = 0; i < N; i++)
          for(std::size_t k = 0; k < bc[i].size(); k++)
            Bt[i][k * C + c] = bc[i][k];
      }
  }

  float wmin = std::numeric_limits<float>::infinity(), bmax = 0.f;
  for(std::size_t i = 0; i < N; i++)
  {
    const auto& p = P[i];
    const std::size_t n = std::size_t(p.width) * p.height;
    r.alpha[i].assign(n * C, 0.f);
    r.beta[i].assign(n * C, 0.f);
    for(std::size_t k = 0; k < n; k++)
    {
      if(!p.valid(int(k)))
        continue;
      const float b = part[i][k];
      float bl[3]{};
      for(int c = 0; c < C; c++)
      {
        const std::size_t kk = k * C + c;
        if(Lb[i][kk] <= 0.f)
          continue;
        const float wt = std::max(kc[c] * Y[i][k], Bt[i][kk]);
        float a = b * (wt - Bt[i][kk]) / Lb[i][kk];
        float be = b * (Bt[i][kk] - B[i][kk]) / Lb[i][kk];
        a = std::clamp(a, 0.f, 1.f);
        be = std::clamp(be, 0.f, 1.f - a);
        r.alpha[i][kk] = a;
        r.beta[i][kk] = be;
        bl[c] = Bt[i][kk];
      }
      wmin = std::min(wmin, Y[i][k]);
      bmax = std::max(bmax, C == 1 ? bl[0] : detail::luma(bl));
    }
  }
  r.whiteTarget = std::isfinite(wmin) ? wmin : 0.f;
  r.blackTarget = bmax;

  r.status = "Done";
  return r;
}
}
