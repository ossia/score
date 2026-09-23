#pragma once
#include <ossia/detail/pod_vector.hpp>

#include <QLineF>
#include <QPointF>

#include <score_plugin_curve_export.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace Curve
{
//! The lowest and highest y over any range of a sequence, in O(log N): each
//! level holds the extremes of pairs of blocks of the level below.
class SCORE_PLUGIN_CURVE_EXPORT MinMaxPyramid
{
public:
  float value(std::size_t i) const noexcept { return m_y[i]; }

  //! y(i) for i in [0, n).
  template <typename Y>
  void build(std::size_t n, Y&& y)
  {
    m_y.resize(n);
    for(std::size_t i = 0; i < n; i++)
      m_y[i] = float(y(i));
    buildLevels();
  }

  //! Over [first, last), which must not be empty.
  std::pair<float, float> range(std::size_t first, std::size_t last) const noexcept;

private:
  void buildLevels();

  struct Extremes
  {
    float min, max;
  };
  ossia::pod_vector<float> m_y;
  std::vector<ossia::pod_vector<Extremes>> m_levels;
};

namespace detail
{
//! First index in [from, n) whose x is >= v. Galloping from `from`: the
//! columns walk forward, so each search costs the log of its own step.
template <typename X>
std::size_t seek(const X& x, std::size_t n, std::size_t from, double v)
{
  std::size_t lo = from, step = 1;
  std::size_t hi = from;
  while(hi < n && x(hi) < v)
  {
    lo = hi + 1;
    hi = from + step;
    step *= 2;
  }
  hi = std::min(hi, n);
  while(lo < hi)
  {
    const std::size_t mid = lo + (hi - lo) / 2;
    if(x(mid) < v)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
}
}

//! A polyline through x-ordered points, `columns` columns across [x0, x1]:
//! each column's points if it has at most two, else its first, lowest, highest
//! and last. O(columns log N). Point i is `pyramid_offset + i` in the pyramid.
template <typename X, typename Y>
void envelope(
    std::size_t n, const X& x, const Y& y, const MinMaxPyramid& pyramid,
    std::size_t pyramid_offset, double x0, double x1, int columns,
    std::vector<QPointF>& out)
{
  out.clear();
  if(n == 0 || columns <= 0 || !(x1 > x0))
    return;

  const std::size_t begin = detail::seek(x, n, 0, x0);
  const std::size_t end = std::min(n, detail::seek(x, n, begin, x1) + 1);
  if(begin > 0)
    out.emplace_back(x(begin - 1), y(begin - 1));

  const double width = (x1 - x0) / columns;
  std::size_t i = begin;
  for(int c = 0; c < columns && i < end; c++)
  {
    const double col_end = c + 1 == columns ? x1 : x0 + (c + 1) * width;
    const std::size_t j = std::min(end, detail::seek(x, n, i, col_end));
    if(j - i <= 2)
    {
      for(std::size_t k = i; k < j; k++)
        out.emplace_back(x(k), y(k));
    }
    else
    {
      const auto [lo, hi] = pyramid.range(pyramid_offset + i, pyramid_offset + j);
      const double first = y(i), last = y(j - 1);
      const double mid = x0 + (c + 0.5) * width;
      out.emplace_back(x(i), first);
      // In the order the column goes: down then up, or up then down.
      if(first <= last)
      {
        out.emplace_back(mid, lo);
        out.emplace_back(mid, hi);
      }
      else
      {
        out.emplace_back(mid, hi);
        out.emplace_back(mid, lo);
      }
      out.emplace_back(x(j - 1), last);
    }
    i = j;
  }
  for(; i < end; i++)
    out.emplace_back(x(i), y(i));
  if(end < n)
    out.emplace_back(x(end), y(end));
}

//! As a waveform: one vertical line per column, across its points and the
//! line through them where it crosses the column's edges, so that columns join
//! and a gap wider than a column is drawn as the ramp it is. Column k spans
//! [x0 + k * width, x0 + (k + 1) * width). O(columns log N).
template <typename X, typename Y>
void envelopeColumns(
    std::size_t n, const X& x, const Y& y, const MinMaxPyramid& pyramid,
    std::size_t pyramid_offset, double x0, double width, int columns,
    std::vector<QLineF>& out)
{
  out.clear();
  if(n == 0 || columns <= 0 || !(width > 0.))
    return;
  out.reserve(columns);

  // The line between points k - 1 and k at v, for 0 < k < n.
  auto at = [&](std::size_t k, double v) {
    const double xa = x(k - 1), xb = x(k);
    const double t = xb > xa ? (v - xa) / (xb - xa) : 1.;
    return y(k - 1) + t * (y(k) - y(k - 1));
  };

  std::size_t i = detail::seek(x, n, 0, x0);
  for(int c = 0; c < columns; c++)
  {
    const double a = x0 + c * width;
    const double b = a + width;
    const double mid = a + 0.5 * width;
    const std::size_t j = detail::seek(x, n, i, b);

    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    if(j > i)
      std::tie(lo, hi) = pyramid.range(pyramid_offset + i, pyramid_offset + j);
    if(i > 0 && i < n)
    {
      const double v = at(i, a);
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
    if(j > 0 && j < n)
    {
      const double v = at(j, b);
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
    if(lo <= hi)
      out.emplace_back(mid, lo, mid, hi);
    i = j;
  }
}

//! Columns on the device's pixel grid, whatever part of the item is repainted:
//! a partial repaint must bucket the points as the full paint did.
struct PixelColumns
{
  double first{};  // in item coordinates, on the grid
  double width{};  // in item coordinates: one device pixel
  int count{};
};
SCORE_PLUGIN_CURVE_EXPORT
PixelColumns pixelColumns(double left, double right, double devicePixelsPerUnit);
}
