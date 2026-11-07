#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Nodes
{
//! The rate of the musical grid (1 a whole note, 4 a quarter...) for steps of
//! `seconds` at `tempo`; 0 when there is none.
//!
//! A synced time reaches an object as seconds in a float, a few millionths
//! off its note value: steps that much short or long drift off the beat by a
//! frame every few minutes. With `note_value`, the step is rounded to the
//! note value it stands for: every note value, tuplets of 3, 5, 7 and 9 and
//! dots down to 1/256, is a whole number of 1/(256 * 9 * 5 * 7) of a whole
//! note.
inline double grid_rate(double seconds, double tempo, bool note_value) noexcept
{
  if(!(seconds > 0.) || !(tempo > 0.))
    return 0.;
  double whole_notes = seconds * tempo / 240.;
  if(note_value)
  {
    constexpr double unit = 256. * 9. * 5. * 7.;
    whole_notes = std::max(1., std::round(whole_notes * unit)) / unit;
  }
  return 1. / whole_notes;
}

//! Calls f(position, index in the bar) for each point in [start; end[ of a
//! pulse of `unit` quarters from `origin` (the last signature change), which
//! goes on across the bar lines; positions in quarters. The index counts from
//! the bar line the point falls in, bars of `quarters_in_bar`. At most 1024
//! points.
//!
//! Each point is computed as origin + k * unit in every tick, so the
//! half-open comparisons put it in exactly one tick.
template <typename F>
void for_each_pulse_point(
    double start, double end, double origin, double unit, double quarters_in_bar,
    F&& f) noexcept
{
  if(!(end > start) || !(unit > 0.) || !(quarters_in_bar > 0.))
    return;
  const auto point = [&](int64_t k) { return origin + double(k) * unit; };
  auto k = int64_t(std::ceil((start - origin) / unit));
  while(point(k - 1) >= start)
    k--;
  while(point(k) < start)
    k++;

  // The rate arrives rounded, so a step meant for a bar line can land a few
  // millionths of a step from it: that close, it is on it.
  constexpr double tolerance = 1e-4;
  for(int count = 0; count < 1024; count++, k++)
  {
    const double p = point(k);
    if(p >= end)
      break;
    const double bar
        = origin
          + std::floor((p - origin) / quarters_in_bar + tolerance) * quarters_in_bar;
    const auto first_in_bar = int64_t(std::ceil((bar - origin) / unit - tolerance));
    f(p, k - first_in_bar);
  }
}
}
