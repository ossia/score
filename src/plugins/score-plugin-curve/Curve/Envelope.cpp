#include "Envelope.hpp"

#include <cmath>
#include <limits>

namespace Curve
{
void MinMaxPyramid::buildLevels()
{
  m_levels.clear();
  const std::size_t n = m_y.size();
  if(n < 2)
    return;

  // Level 1 from the values, each next one from the one below.
  auto& first = m_levels.emplace_back();
  first.resize((n + 1) / 2);
  for(std::size_t b = 0; b < first.size(); b++)
  {
    const float a = m_y[2 * b];
    const float c = 2 * b + 1 < n ? m_y[2 * b + 1] : a;
    first[b] = {std::min(a, c), std::max(a, c)};
  }

  while(m_levels.back().size() > 1)
  {
    const auto& below = m_levels.back();
    ossia::pod_vector<Extremes> level((below.size() + 1) / 2);
    for(std::size_t b = 0; b < level.size(); b++)
    {
      const auto a = below[2 * b];
      const auto c = 2 * b + 1 < below.size() ? below[2 * b + 1] : a;
      level[b] = {std::min(a.min, c.min), std::max(a.max, c.max)};
    }
    m_levels.push_back(std::move(level));
  }
}

std::pair<float, float>
MinMaxPyramid::range(std::size_t l, std::size_t r) const noexcept
{
  float lo = std::numeric_limits<float>::max();
  float hi = std::numeric_limits<float>::lowest();
  auto take = [&](std::size_t level, std::size_t i) {
    if(level == 0)
    {
      lo = std::min(lo, m_y[i]);
      hi = std::max(hi, m_y[i]);
    }
    else
    {
      const auto& b = m_levels[level - 1][i];
      lo = std::min(lo, b.min);
      hi = std::max(hi, b.max);
    }
  };

  // Bottom-up, as in a segment tree: the unpaired blocks at each end are
  // taken at that level, the rest goes up.
  for(std::size_t level = 0; l < r; level++)
  {
    if(l & 1)
      take(level, l++);
    if(r & 1)
      take(level, --r);
    l >>= 1;
    r >>= 1;
  }
  return {lo, hi};
}

PixelColumns pixelColumns(double left, double right, double devicePixelsPerUnit)
{
  PixelColumns c;
  if(!(devicePixelsPerUnit > 0.) || !(right > left))
    return c;
  c.width = 1. / devicePixelsPerUnit;
  c.first = std::floor(left * devicePixelsPerUnit) * c.width;
  c.count = int(std::ceil((right - c.first) * devicePixelsPerUnit));
  return c;
}
}
