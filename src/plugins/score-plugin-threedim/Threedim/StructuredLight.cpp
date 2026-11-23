#include "StructuredLight.hpp"

#include <Gfx/Calibration/PairCode.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Threedim::sl
{

static int slCeilLog2(int n) noexcept
{
  int b = 0;
  while((1 << b) < n)
    b++;
  return b;
}

GraySequence GraySequence::make(int width, int height, int minStripe) noexcept
{
  GraySequence s;
  s.width = std::max(1, width);
  s.height = std::max(1, height);
  s.bitsX = std::max(1, slCeilLog2(s.width));
  s.bitsY = std::max(1, slCeilLog2(s.height));
  int skip = 0;
  while((2 << skip) < minStripe)
    skip++;
  s.skip = std::min(skip, std::min(s.bitsX, s.bitsY) - 1);
  return s;
}

int GraySequence::autoPeriod(int skip) noexcept
{
  const int want = 4 * (2 << skip);
  int p = 1;
  while(p < want)
    p <<= 1;
  return p;
}

void GraySequence::enablePhase(int steps, int p, float g) noexcept
{
  phaseSteps = std::max(0, steps);
  period = p > 0 ? p : autoPeriod(skip);
  gamma = g > 0.f ? g : 1.f;
}

float GraySequence::value(int k, int u, int v) const noexcept
{
  if(!isPhase(k))
    return lit(k, u, v) ? 1.f : 0.f;
  const int j = k - grayCount();
  const int axisCoord = j < phaseSteps ? u : v;
  const int step = j % phaseSteps;
  const double a = 2. * M_PI * double(axisCoord) / period - 2. * M_PI * step / phaseSteps;
  return float(0.5 + 0.5 * std::cos(a));
}

bool GraySequence::lit(int k, int u, int v) const noexcept
{
  if(isPhase(k))
    return value(k, u, v) >= 0.5f;
  if(k <= 0)
    return true;
  if(k == 1)
    return false;
  const int j = k - 2;
  int b = j / 2;
  const bool inverse = (j % 2) == 1;
  int code, bit;
  if(b < projectedX())
  {
    code = u ^ (u >> 1);
    bit = bitsX - 1 - b;
  }
  else
  {
    b -= projectedX();
    if(b >= projectedY())
      return false;
    code = v ^ (v >> 1);
    bit = bitsY - 1 - b;
  }
  const bool one = ((code >> bit) & 1) == 1;
  return one != inverse;
}

void renderPattern(const GraySequence& seq, int k, float level, uint8_t* rgba)
{
  const float lv = std::clamp(level, 0.f, 1.f);
  const uint8_t on = uint8_t(std::lround(lv * 255.f));
  if(seq.isPhase(k))
  {
    const int j = k - seq.grayCount();
    const bool alongU = j < seq.phaseSteps;
    const int n = alongU ? seq.width : seq.height;
    std::vector<uint8_t> lut(n);
    for(int c = 0; c < n; c++)
    {
      const float val = alongU ? seq.value(k, c, 0) : seq.value(k, 0, c);
      lut[c] = uint8_t(
          std::lround(255.f * lv * std::pow(std::max(val, 0.f), 1.f / seq.gamma)));
    }
    for(int v = 0; v < seq.height; v++)
    {
      uint8_t* row = rgba + std::size_t(v) * seq.width * 4;
      for(int u = 0; u < seq.width; u++)
      {
        const uint8_t c = lut[alongU ? u : v];
        row[u * 4 + 0] = row[u * 4 + 1] = row[u * 4 + 2] = c;
        row[u * 4 + 3] = 255;
      }
    }
    return;
  }
  for(int v = 0; v < seq.height; v++)
  {
    uint8_t* row = rgba + std::size_t(v) * seq.width * 4;
    for(int u = 0; u < seq.width; u++)
    {
      const uint8_t c = seq.lit(k, u, v) ? on : 0;
      row[u * 4 + 0] = row[u * 4 + 1] = row[u * 4 + 2] = c;
      row[u * 4 + 3] = 255;
    }
  }
}

void renderTestGrid(int width, int height, float level, uint8_t* rgba)
{
  const uint8_t on = uint8_t(std::lround(std::clamp(level, 0.f, 1.f) * 255.f));
  for(int v = 0; v < height; v++)
    for(int u = 0; u < width; u++)
    {
      const bool line = (u % 64) == 0 || (v % 64) == 0 || u == width - 1
                        || v == height - 1 || std::abs(u - width / 2) <= 1
                        || std::abs(v - height / 2) <= 1 || (u < 32 && v < 32);
      uint8_t* p = rgba + (std::size_t(v) * width + u) * 4;
      p[0] = p[1] = p[2] = line ? on : 0;
      p[3] = 255;
    }
}

void Decoder::reset(int width, int height, const GraySequence& seq)
{
  m_w = width;
  m_h = height;
  m_seq = seq;
  const std::size_t n = std::size_t(width) * height;
  m_white.assign(n, 0.f);
  m_black.assign(n, 0.f);
  m_hasWhite = m_hasBlack = false;
  m_pending.assign(seq.grayCount(), {});
  m_added.assign(seq.count(), 0);
  m_saturated.assign(n, 0);
  m_error.clear();
  for(auto* acc : {&m_sinU, &m_cosU, &m_sinV, &m_cosV})
    acc->assign(seq.phaseSteps > 0 ? n : 0, 0.f);
  for(Axis* a : {&m_u, &m_v})
  {
    a->bits.assign(n, 0);
    a->faint.assign(n, 0);
    a->minContrast.assign(n, std::numeric_limits<float>::infinity());
  }
}

void Decoder::markSaturated(const std::vector<uint8_t>& clipped)
{
  if(clipped.size() != m_saturated.size())
    return;
  for(std::size_t i = 0; i < clipped.size(); i++)
    m_saturated[i] |= clipped[i];
}

void Decoder::addPair(int b, const std::vector<float>& I, const std::vector<float>& Iinv)
{
  const bool alongU = b < m_seq.projectedX();
  Axis& a = alongU ? m_u : m_v;
  const int pos = alongU ? m_seq.bitsX - 1 - b : m_seq.bitsY - 1 - (b - m_seq.projectedX());
  const uint32_t mask = 1u << pos;
  const std::size_t n = a.bits.size();
  for(std::size_t i = 0; i < n; i++)
  {
    const auto bit = score::gfx::paircode::read(
        I[i], Iinv[i], m_white[i] - m_black[i], m_contrast);
    if(bit.one)
      a.bits[i] |= mask;
    if(!bit.clear)
      a.faint[i] |= mask;
    else
      a.minContrast[i] = std::min(a.minContrast[i], bit.contrast);
  }
}

void Decoder::flush()
{
  if(!m_hasWhite || !m_hasBlack)
    return;
  const int bits = m_seq.projectedX() + m_seq.projectedY();
  for(int b = 0; b < bits; b++)
  {
    auto& I = m_pending[2 + 2 * b];
    auto& Iinv = m_pending[3 + 2 * b];
    if(I.empty() || Iinv.empty())
      continue;
    addPair(b, I, Iinv);
    I = {};
    Iinv = {};
  }
}

bool Decoder::add(int k, const std::vector<float>& lum)
{
  if(k < 0 || k >= m_seq.count())
  {
    m_error = "frame " + std::to_string(k) + " is not a frame of the sequence";
    return false;
  }
  if(lum.size() != std::size_t(m_w) * m_h)
  {
    m_error = "frame " + std::to_string(k) + " is not " + std::to_string(m_w) + " x "
              + std::to_string(m_h);
    return false;
  }
  if(m_added[k])
  {
    m_error = "frame " + std::to_string(k) + " was added twice";
    return false;
  }
  m_added[k] = 1;
  if(k == 0 || k == 1)
  {
    (k == 0 ? m_white : m_black) = lum;
    (k == 0 ? m_hasWhite : m_hasBlack) = true;
    flush();
    return true;
  }
  if(m_seq.isPhase(k))
  {
    const int j = k - m_seq.grayCount();
    const bool alongU = j < m_seq.phaseSteps;
    const double d = 2. * M_PI * (j % m_seq.phaseSteps) / m_seq.phaseSteps;
    const float c = float(std::cos(d)), sn = float(std::sin(d));
    auto& S = alongU ? m_sinU : m_sinV;
    auto& C = alongU ? m_cosU : m_cosV;
    for(std::size_t i = 0; i < lum.size(); i++)
    {
      S[i] += lum[i] * sn;
      C[i] += lum[i] * c;
    }
    return true;
  }
  m_pending[k] = lum;
  flush();
  return true;
}

std::vector<int> Decoder::missing() const
{
  std::vector<int> out;
  for(int k = 0; k < int(m_added.size()); k++)
    if(!m_added[k])
      out.push_back(k);
  return out;
}

std::vector<DecodedPixel> Decoder::decode(float minSignal, int minBits) const
{
  const std::size_t n = std::size_t(m_w) * m_h;
  std::vector<DecodedPixel> out(n);
  if(!missing().empty())
    return out;
  auto coord = [](uint32_t gray, int known, int total) {
    uint32_t binary = gray;
    for(uint32_t shift = gray >> 1; shift; shift >>= 1)
      binary ^= shift;
    const int rest = total - known;
    return float(binary << rest) + float((1u << rest) - 1u) * 0.5f;
  };
  auto knownBits = [](uint32_t faint, int total, int projected) {
    int seen = 0;
    for(int b = 0; b < projected; b++)
      if((faint >> (total - 1 - b)) & 1u)
        if(++seen == 2)
          return b;
    return projected;
  };
  const int needU = std::min(minBits, m_seq.projectedX());
  const int needV = std::min(minBits, m_seq.projectedY());
  std::vector<uint8_t> lit(n);
  for(std::size_t i = 0; i < n; i++)
    lit[i] = m_white[i] - m_black[i] > minSignal;
  constexpr int rim = 2;
  std::vector<uint8_t> interior(n, 0);
  for(int y = 0; y < m_h; y++)
    for(int x = 0; x < m_w; x++)
    {
      bool ok = lit[std::size_t(y) * m_w + x];
      for(int dy = -rim; ok && dy <= rim; dy++)
        for(int dx = -rim; ok && dx <= rim; dx++)
        {
          const int xx = x + dx, yy = y + dy;
          if(xx >= 0 && yy >= 0 && xx < m_w && yy < m_h && !lit[std::size_t(yy) * m_w + xx])
            ok = false;
        }
      interior[std::size_t(y) * m_w + x] = ok;
    }
  for(std::size_t i = 0; i < n; i++)
  {
    auto& o = out[i];
    if(!lit[i])
      continue;
    o.rim = !interior[i];
    o.saturated = m_saturated[i];
    const int ku = knownBits(m_u.faint[i], m_seq.bitsX, m_seq.projectedX());
    const int kv = knownBits(m_v.faint[i], m_seq.bitsY, m_seq.projectedY());
    if(ku < needU || kv < needV)
      continue;
    o.u = coord(m_u.bits[i] >> (m_seq.bitsX - ku), ku, m_seq.bitsX);
    o.v = coord(m_v.bits[i] >> (m_seq.bitsY - kv), kv, m_seq.bitsY);
    if(m_seq.phaseSteps > 0 && !o.saturated)
    {
      const float span = std::max(m_white[i] - m_black[i], 1e-6f);
      const float P = float(m_seq.period);
      auto unwrap = [&](float gray, int known, int total, float S, float C, float& out) {
        const float block = float(1 << (total - known));
        if(1.5f * block >= P * 0.5f)
          return false;
        const float mod = 2.f / m_seq.phaseSteps * std::sqrt(S * S + C * C);
        if(mod < m_minModulation * span)
          return false;
        float ph = std::atan2(S, C);
        if(ph < 0.f)
          ph += float(2. * M_PI);
        const float within = ph * P / float(2. * M_PI);
        const float m = std::floor(gray / P);
        float best = gray, bestErr = std::numeric_limits<float>::max();
        for(float k : {m - 1.f, m, m + 1.f})
        {
          const float cand = k * P + within;
          const float e = std::abs(cand - gray);
          if(e < bestErr)
          {
            bestErr = e;
            best = cand;
          }
        }
        out = best;
        return true;
      };
      o.phaseU = unwrap(o.u, ku, m_seq.bitsX, m_sinU[i], m_cosU[i], o.u);
      o.phaseV = unwrap(o.v, kv, m_seq.bitsY, m_sinV[i], m_cosV[i], o.v);
    }
    if(o.u < -0.5f || o.v < -0.5f || o.u >= m_seq.width || o.v >= m_seq.height)
      continue;
    o.confidence = std::min(m_u.minContrast[i], m_v.minContrast[i]);
    if(!std::isfinite(o.confidence))
      o.confidence = 1.f;
    o.valid = true;
  }
  return out;
}

void SparseMap::reset(int w, int h)
{
  width = w;
  height = h;
  value.assign(std::size_t(w) * h, {0.f, 0.f, 0.f});
  weight.assign(std::size_t(w) * h, 0.f);
}

void SparseMap::keepBest(int u, int v, const std::array<float, 3>& x, float confidence)
{
  if(u < 0 || v < 0 || u >= width || v >= height)
    return;
  const std::size_t i = std::size_t(v) * width + u;
  if(confidence > weight[i])
  {
    weight[i] = std::max(confidence, 1e-6f);
    value[i] = x;
  }
}

void SparseMap::accumulate(int u, int v, const std::array<float, 3>& x)
{
  if(u < 0 || v < 0 || u >= width || v >= height)
    return;
  const std::size_t i = std::size_t(v) * width + u;
  auto& a = value[i];
  a[0] += x[0];
  a[1] += x[1];
  a[2] += x[2];
  weight[i] += 1.f;
}

void SparseMap::splat(float u, float v, const std::array<float, 3>& x, float w)
{
  const int u0 = int(std::floor(u)), v0 = int(std::floor(v));
  const float fu = u - u0, fv = v - v0;
  for(int dv = 0; dv < 2; dv++)
    for(int du = 0; du < 2; du++)
    {
      const int uu = u0 + du, vv = v0 + dv;
      if(uu < 0 || vv < 0 || uu >= width || vv >= height)
        continue;
      const float b = w * (du ? fu : 1.f - fu) * (dv ? fv : 1.f - fv);
      if(b <= 0.f)
        continue;
      const std::size_t i = std::size_t(vv) * width + uu;
      value[i][0] += b * x[0];
      value[i][1] += b * x[1];
      value[i][2] += b * x[2];
      weight[i] += b;
    }
}

namespace
{
struct PushPullLevel
{
  int w{}, h{};
  std::vector<std::array<float, 3>> v;
  std::vector<uint8_t> has;
};

float slDist2(const std::array<float, 3>& a, const std::array<float, 3>& b) noexcept
{
  const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return dx * dx + dy * dy + dz * dz;
}
}

std::vector<float>
pushPullFill(const SparseMap& map, int holeRadius, float discontinuity, bool average)
{
  const int W = map.width, H = map.height;
  std::vector<float> out(std::size_t(W) * H * 4, 0.f);
  if(W <= 0 || H <= 0)
    return out;

  std::vector<PushPullLevel> levels(1);
  levels[0].w = W;
  levels[0].h = H;
  levels[0].v.resize(std::size_t(W) * H);
  levels[0].has.resize(std::size_t(W) * H);
  for(std::size_t i = 0; i < levels[0].v.size(); i++)
  {
    const float w = map.weight[i];
    levels[0].has[i] = w > 0.f;
    auto x = map.value[i];
    if(average && w > 0.f)
      for(auto& c : x)
        c /= w;
    levels[0].v[i] = x;
  }

  const float lim2 = discontinuity * discontinuity;
  while(levels.back().w > 1 || levels.back().h > 1)
  {
    const PushPullLevel& f = levels.back();
    PushPullLevel c;
    c.w = (f.w + 1) / 2;
    c.h = (f.h + 1) / 2;
    c.v.assign(std::size_t(c.w) * c.h, {0.f, 0.f, 0.f});
    c.has.assign(std::size_t(c.w) * c.h, 0);
    for(int y = 0; y < c.h; y++)
      for(int x = 0; x < c.w; x++)
      {
        std::array<float, 3> s[4];
        int n = 0;
        for(int dy = 0; dy < 2; dy++)
          for(int dx = 0; dx < 2; dx++)
          {
            const int fx = 2 * x + dx, fy = 2 * y + dy;
            if(fx < f.w && fy < f.h && f.has[std::size_t(fy) * f.w + fx])
              s[n++] = f.v[std::size_t(fy) * f.w + fx];
          }
        if(n == 0)
          continue;
        int ref = 0;
        if(lim2 > 0.f && n > 2)
        {
          float best = std::numeric_limits<float>::max();
          for(int a = 0; a < n; a++)
          {
            float sum = 0.f;
            for(int b = 0; b < n; b++)
              sum += slDist2(s[a], s[b]);
            if(sum < best)
            {
              best = sum;
              ref = a;
            }
          }
        }
        std::array<float, 3> acc{0.f, 0.f, 0.f};
        int used = 0;
        for(int a = 0; a < n; a++)
        {
          if(lim2 > 0.f && slDist2(s[a], s[ref]) > lim2)
            continue;
          acc[0] += s[a][0];
          acc[1] += s[a][1];
          acc[2] += s[a][2];
          used++;
        }
        auto& dst = c.v[std::size_t(y) * c.w + x];
        dst = {acc[0] / used, acc[1] / used, acc[2] / used};
        c.has[std::size_t(y) * c.w + x] = 1;
      }
    levels.push_back(std::move(c));
  }

  for(int l = int(levels.size()) - 2; l >= 0; l--)
  {
    PushPullLevel& f = levels[l];
    const PushPullLevel& c = levels[l + 1];
    for(int y = 0; y < f.h; y++)
      for(int x = 0; x < f.w; x++)
      {
        const std::size_t i = std::size_t(y) * f.w + x;
        if(f.has[i])
          continue;
        const std::size_t ci = std::size_t(y / 2) * c.w + x / 2;
        if(!c.has[ci])
          continue;
        const std::array<float, 3> nearest = c.v[ci];
        const float fx = (x + 0.5f) * 0.5f - 0.5f, fy = (y + 0.5f) * 0.5f - 0.5f;
        const int x0 = int(std::floor(fx)), y0 = int(std::floor(fy));
        std::array<float, 3> acc{0.f, 0.f, 0.f};
        float wsum = 0.f;
        for(int dy = 0; dy < 2; dy++)
          for(int dx = 0; dx < 2; dx++)
          {
            const int cx = x0 + dx, cy = y0 + dy;
            if(cx < 0 || cy < 0 || cx >= c.w || cy >= c.h)
              continue;
            const std::size_t k = std::size_t(cy) * c.w + cx;
            if(!c.has[k])
              continue;
            if(lim2 > 0.f && slDist2(c.v[k], nearest) > lim2)
              continue;
            const float w = (dx ? fx - x0 : 1.f - (fx - x0)) * (dy ? fy - y0 : 1.f - (fy - y0));
            acc[0] += w * c.v[k][0];
            acc[1] += w * c.v[k][1];
            acc[2] += w * c.v[k][2];
            wsum += w;
          }
        f.v[i] = wsum > 1e-6f ? std::array<float, 3>{acc[0] / wsum, acc[1] / wsum, acc[2] / wsum}
                              : nearest;
        f.has[i] = 1;
      }
  }

  std::vector<int> integral(std::size_t(W + 1) * (H + 1), 0);
  for(int y = 0; y < H; y++)
  {
    int rowSum = 0;
    for(int x = 0; x < W; x++)
    {
      rowSum += map.weight[std::size_t(y) * W + x] > 0.f ? 1 : 0;
      integral[std::size_t(y + 1) * (W + 1) + x + 1]
          = integral[std::size_t(y) * (W + 1) + x + 1] + rowSum;
    }
  }
  auto boxCount = [&](int x0, int y0, int x1, int y1) {
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    x1 = std::min(x1, W - 1);
    y1 = std::min(y1, H - 1);
    if(x1 < x0 || y1 < y0)
      return 0;
    const auto at = [&](int x, int y) { return integral[std::size_t(y) * (W + 1) + x]; };
    return at(x1 + 1, y1 + 1) - at(x0, y1 + 1) - at(x1 + 1, y0) + at(x0, y0);
  };

  const PushPullLevel& f = levels[0];
  const int r = std::max(0, holeRadius);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const std::size_t i = std::size_t(y) * W + x;
      const bool measured = map.weight[i] > 0.f;
      if(!f.has[i] || (!measured && boxCount(x - r, y - r, x + r, y + r) == 0))
        continue;
      float* o = out.data() + i * 4;
      o[0] = f.v[i][0];
      o[1] = f.v[i][1];
      o[2] = f.v[i][2];
      o[3] = 1.f;
    }
  return out;
}
}
