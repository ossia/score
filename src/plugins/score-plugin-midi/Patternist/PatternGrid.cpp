#include "PatternGrid.hpp"

#include <ossia/detail/flicks.hpp>

#include <cmath>
#include <limits>

namespace Patternist
{
std::vector<StepSlot> timelineSlots(
    const Pattern& p, const TimeGrid& grid, ZoomRatio zoom, double width,
    std::size_t maxSlots)
{
  std::vector<StepSlot> res;
  if(zoom <= 0. || width <= 0. || p.length <= 0 || p.division <= 0)
    return res;

  constexpr double eps = 1e-6;
  const double quarter = ossia::quarter_duration<double>;
  const double unit = 4. / p.division * quarter;
  const double t0 = grid.origin.impl;
  const double t1 = t0 + width * zoom;
  if((t1 - t0) / unit > double(maxSlots))
    return res;

  struct Section
  {
    double start;
    double end;
    ossia::time_signature sig;
  };
  std::vector<Section> sections;
  {
    constexpr double inf = std::numeric_limits<double>::infinity();
    const auto& sigs = grid.signatures;
    if(sigs.empty() || sigs.front().first.impl > 0)
      sections.push_back(
          {0., sigs.empty() ? inf : double(sigs.front().first.impl), {4, 4}});
    for(std::size_t i = 0; i < sigs.size(); i++)
    {
      const double end = i + 1 < sigs.size() ? double(sigs[i + 1].first.impl) : inf;
      auto sig = sigs[i].second;
      if(sig.upper == 0 || sig.lower == 0)
        sig = {4, 4};
      sections.push_back({double(sigs[i].first.impl), end, sig});
    }
  }

  struct Point
  {
    double t;
    bool bar;
    bool beat;
  };
  std::vector<Point> points;
  for(const auto& sec : sections)
  {
    if(sec.end <= t0 || sec.start >= t1)
      continue;
    const double bar = 4. * sec.sig.upper / sec.sig.lower * quarter;
    const double beat = 4. / sec.sig.lower * quarter;
    double k = std::max(0., std::floor((t0 - sec.start) / bar));
    for(double bar_line = sec.start + k * bar; bar_line < std::min(sec.end, t1);
        bar_line = sec.start + (++k) * bar)
    {
      const double next_bar = std::min(bar_line + bar, sec.end);
      for(int j = 0;; j++)
      {
        const double t = bar_line + j * unit;
        if(t >= next_bar - eps || t >= t1)
          break;
        if(t < t0)
          continue;
        const double inBeats = (t - bar_line) / beat;
        points.push_back({t, j == 0, std::abs(inBeats - std::round(inBeats)) < 1e-6});
        if(points.size() > maxSlots)
          return res;
      }
    }
  }

  // The engine starts on step 0 at the first point from playStart on.
  std::ptrdiff_t first = points.size();
  for(std::size_t i = 0; i < points.size(); i++)
  {
    if(points[i].t >= grid.playStart.impl - eps)
    {
      first = i;
      break;
    }
  }

  res.reserve(points.size());
  for(std::size_t i = 0; i < points.size(); i++)
  {
    const auto& pt = points[i];
    const double x0 = (pt.t - t0) / zoom;
    const double x1 = i + 1 < points.size() ? (points[i + 1].t - t0) / zoom
                                            : std::min(width, x0 + unit / zoom);
    const std::ptrdiff_t n = std::ptrdiff_t(i) - first;
    const int step = int(((n % p.length) + p.length) % p.length);
    res.push_back({x0, x1, step, n < 0 || n >= p.length, pt.bar, pt.beat});
  }
  return res;
}
}
