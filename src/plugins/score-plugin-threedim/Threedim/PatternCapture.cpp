#include "PatternCapture.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Threedim
{
std::array<std::vector<float>, 3> PatternCapture::gammaTable(float gamma, int size)
{
  std::array<std::vector<float>, 3> t;
  const float g = gamma > 0.f ? gamma : 1.f;
  for(auto& c : t)
  {
    c.resize(std::max(2, size));
    for(std::size_t i = 0; i < c.size(); i++)
      c[i] = std::pow(float(i) / float(c.size() - 1), g);
  }
  return t;
}

void PatternCapture::reset() noexcept
{
  m_w = m_h = 0;
  m_hasPrevious = false;
  m_lastSerial = 0;
  m_lastAt = -1e300;
  m_state = State::Done;
}

void PatternCapture::begin(double now, const Options& o)
{
  m_opt = o;
  m_opt.frames = std::max(1, o.frames);
  m_state = State::Settling;
  m_shownAt = now;
  m_taken = 0;
  m_accepted = false;
  m_lum.clear();
  m_rgb.clear();
  m_clipped.clear();
}

float PatternCapture::linear(int c, float v) const noexcept
{
  if(!m_opt.linearise)
    return v;
  const auto& t = (*m_opt.linearise)[c];
  if(t.size() < 2)
    return v;
  const float x = std::clamp(v, 0.f, 1.f) * float(t.size() - 1);
  const std::size_t i = std::min(std::size_t(x), t.size() - 2);
  const float f = x - float(i);
  return t[i] + (t[i + 1] - t[i]) * f;
}

uint64_t PatternCapture::signature(const CameraFrame& f) const noexcept
{
  const std::size_t n = std::size_t(f.width) * f.height * 4;
  const std::size_t step = std::max<std::size_t>(1, n / 4096) | 1;
  uint64_t h = 1469598103934665603ull;
  for(std::size_t i = 0; i < n; i += step)
  {
    uint32_t b;
    std::memcpy(&b, f.rgba + i, 4);
    h = (h ^ b) * 1099511628211ull;
  }
  return h;
}

float PatternCapture::sampledMean(const CameraFrame& f) const noexcept
{
  const std::size_t n = std::size_t(f.width) * f.height;
  const std::size_t step = std::max<std::size_t>(1, n / 4096);
  double sum = 0.;
  std::size_t k = 0;
  for(std::size_t i = 0; i < n; i += step, k++)
    for(int c = 0; c < 3; c++)
      sum += linear(c, f.rgba[i * 4 + c]);
  return k ? float(sum / (3. * k)) : 0.f;
}

PatternCapture::State PatternCapture::feed(double now, const CameraFrame& f)
{
  m_accepted = false;
  if(m_state == State::Settling)
  {
    if(now - m_shownAt < m_opt.settleMs)
      return m_state;
    m_state = State::Capturing;
    m_waitFrom = m_shownAt + m_opt.settleMs;
    m_sawFrame = false;
  }
  if(m_state != State::Capturing)
    return m_state;

  const bool has = f.rgba && f.width > 0 && f.height > 0;
  const bool fresh = has
                     && (f.serial ? f.serial != m_lastSerial
                                  : now - m_lastAt >= m_opt.intervalMs);
  if(!fresh)
  {
    if(now - m_waitFrom > m_opt.timeoutMs)
      m_state = m_sawFrame ? State::Unchanged : State::NoFrame;
    return m_state;
  }
  m_sawFrame = true;
  if(m_w == 0)
  {
    m_w = f.width;
    m_h = f.height;
  }
  if(f.width != m_w || f.height != m_h)
  {
    m_state = State::SizeChanged;
    return m_state;
  }
  m_lastSerial = f.serial;
  m_lastAt = now;

  const uint64_t sig = signature(f);
  if(m_taken == 0)
  {
    const bool stale = m_opt.requireChange && m_hasPrevious && sig == m_previous;
    const bool bright = m_opt.darkerThan >= 0.f && sampledMean(f) >= m_opt.darkerThan;
    if(stale || bright)
    {
      if(now - m_waitFrom > m_opt.timeoutMs)
        m_state = State::Unchanged;
      return m_state;
    }
    m_latency = now - m_shownAt;
  }

  const std::size_t n = std::size_t(m_w) * m_h;
  if(m_lum.size() != n)
  {
    m_lum.assign(n, 0.f);
    m_clipped.assign(n, 0);
    if(m_opt.rgb)
      m_rgb.assign(n * 3, 0.f);
  }
  for(std::size_t i = 0; i < n; i++)
  {
    const float* p = f.rgba + i * 4;
    m_lum[i] += (linear(0, p[0]) + linear(1, p[1]) + linear(2, p[2])) * (1.f / 3.f);
    if(p[0] >= m_opt.clipLevel || p[1] >= m_opt.clipLevel || p[2] >= m_opt.clipLevel)
      m_clipped[i] = 1;
    if(m_opt.rgb)
      for(int c = 0; c < 3; c++)
        m_rgb[i * 3 + c] += p[c];
  }
  m_accepted = true;
  m_last = sig;
  m_waitFrom = now;
  m_sawFrame = false;
  if(++m_taken < m_opt.frames)
    return m_state;

  const float inv = 1.f / float(m_taken);
  double sum = 0.;
  for(float& v : m_lum)
  {
    v *= inv;
    sum += v;
  }
  for(float& v : m_rgb)
    v *= inv;
  m_mean = n ? float(sum / double(n)) : 0.f;
  m_previous = m_last;
  m_hasPrevious = true;
  m_state = State::Done;
  return m_state;
}
}
