#include "PatternGrid.hpp"

#include <ossia/detail/flicks.hpp>

#include <algorithm>
#include <cmath>

#include <limits>

namespace Patternist
{
std::vector<StepSlot> timelineSlots(
    int length, int division, const TimeGrid& grid, ZoomRatio zoom, double width,
    std::size_t maxSlots)
{
  std::vector<StepSlot> res;
  if(zoom <= 0. || width <= 0. || length <= 0 || division <= 0)
    return res;

  constexpr double eps = 1e-6;
  const double quarter = ossia::quarter_duration<double>;
  const double unit = 4. / division * quarter;
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
    int beatIndex;
    int shade;
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
        const int beatIndex = int(std::floor(inBeats + 1e-6));
        points.push_back(
            {t, j == 0, std::abs(inBeats - std::round(inBeats)) < 1e-6, beatIndex,
             (unit < beat - eps ? beatIndex : int(k)) % 2});
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
    const int step = int(((n % length) + length) % length);
    res.push_back(
        {x0, x1, step, n < 0 || n >= length, pt.bar, pt.beat, pt.beatIndex, n,
         pt.shade});
  }
  return res;
}

ossia::time_signature signatureAt(const TimeGrid& grid, TimeVal date) noexcept
{
  ossia::time_signature sig{4, 4};
  for(const auto& [t, s] : grid.signatures)
  {
    if(t > date)
      break;
    if(s.upper > 0 && s.lower > 0)
      sig = s;
  }
  return sig;
}

std::vector<StepSlot>
fitSlots(const Pattern& p, ossia::time_signature sig, double x0, double x1)
{
  std::vector<StepSlot> res;
  if(p.length <= 0 || x1 <= x0)
    return res;
  const double w = (x1 - x0) / p.length;

  // Steps per beat, when a beat is a whole number of steps: a beat is a
  // 1/lower note, a step a 1/division note.
  const int stepsPerBeat
      = (sig.lower > 0 && p.division % sig.lower == 0) ? p.division / sig.lower : 0;
  const int stepsPerBar = stepsPerBeat * sig.upper;

  res.reserve(p.length);
  for(int i = 0; i < p.length; i++)
  {
    const bool beat = stepsPerBeat > 0 && i % stepsPerBeat == 0;
    const bool bar = stepsPerBar > 0 && i % stepsPerBar == 0;
    const int beatIndex = stepsPerBeat > 0 ? (i % stepsPerBar) / stepsPerBeat : 0;
    const int shade = stepsPerBeat > 1 ? beatIndex % 2
                      : stepsPerBar > 0 ? (i / stepsPerBar) % 2
                                        : 0;
    res.push_back(
        {x0 + i * w, x0 + (i + 1) * w, i, false, bar, beat, beatIndex, i, shade});
  }
  return res;
}

double longestLoop(const Pattern& p) noexcept
{
  double span = p.division > 0 ? 4. * p.length / p.division : 0.;
  for(const Lane& l : p.lanes)
  {
    const int div = laneDivision(p, l);
    if(div > 0)
      span = std::max(span, 4. * laneLength(p, l) / div);
  }
  return span;
}

std::vector<StepSlot> fitLaneSlots(
    int length, int division, ossia::time_signature sig, double span, double x0,
    double x1)
{
  std::vector<StepSlot> res;
  if(length <= 0 || division <= 0 || span <= 0. || x1 <= x0)
    return res;
  const double w = (x1 - x0) * (4. / division) / span;
  if(w <= 0.)
    return res;

  const int stepsPerBeat
      = (sig.lower > 0 && division % sig.lower == 0) ? division / sig.lower : 0;
  const int stepsPerBar = stepsPerBeat * sig.upper;

  const int count = int(std::ceil((x1 - x0) / w - 1e-6));
  res.reserve(count);
  for(int i = 0; i < count; i++)
  {
    const bool beat = stepsPerBeat > 0 && i % stepsPerBeat == 0;
    const bool bar = stepsPerBar > 0 && i % stepsPerBar == 0;
    const int beatIndex = stepsPerBeat > 0 ? (i % stepsPerBar) / stepsPerBeat : 0;
    const int shade = stepsPerBeat > 1 ? beatIndex % 2
                      : stepsPerBar > 0 ? (i / stepsPerBar) % 2
                                        : 0;
    res.push_back(
        {x0 + i * w, std::min(x1, x0 + (i + 1) * w), i % length, i >= length, bar, beat,
         beatIndex, i, shade});
  }
  return res;
}

FitLayout fitLayout(
    const Pattern& p, ossia::time_signature sig, double x0, double x1, double minStep)
{
  FitLayout res;
  const double span = longestLoop(p);
  bool aligned = span > 0.;
  for(const Lane& l : p.lanes)
  {
    const int div = laneDivision(p, l);
    if(div <= 0 || (x1 - x0) * (4. / div) / span < minStep)
      aligned = false;
  }

  res.span = aligned ? span : 0.;
  res.lanes.reserve(p.lanes.size());
  for(const Lane& l : p.lanes)
  {
    const int len = laneLength(p, l);
    const int div = laneDivision(p, l);
    res.lanes.push_back(fitLaneSlots(
        len, div, sig, aligned ? span : 4. * len / std::max(1, div), x0, x1));
  }
  return res;
}

int playingSlot(
    const std::vector<StepSlot>& slots, int64_t played, int length, int division,
    double span) noexcept
{
  if(played < 0 || length <= 0 || division <= 0 || slots.empty())
    return -1;
  const int step = int(played % length);
  if(span > 0.)
  {
    const double unit = 4. / division;
    const auto at = int64_t(std::floor(std::fmod(played * unit, span) / unit + 1e-6));
    if(at < int64_t(slots.size()) && slots[at].step == step)
      return int(at);
  }
  for(std::size_t i = 0; i < slots.size(); i++)
    if(slots[i].step == step)
      return int(i);
  return -1;
}

int slotWithIndex(const std::vector<StepSlot>& slots, int64_t index) noexcept
{
  auto it = std::lower_bound(
      slots.begin(), slots.end(), index,
      [](const StepSlot& s, int64_t i) { return s.index < i; });
  return it != slots.end() && it->index == index ? int(it - slots.begin()) : -1;
}
}
