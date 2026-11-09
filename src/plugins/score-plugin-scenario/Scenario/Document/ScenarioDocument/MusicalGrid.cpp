#include "MusicalGrid.hpp"

#include <Scenario/Document/Interval/FullView/TimeSignatureItem.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/ssize.hpp>

#include <QLineF>

#include <cmath>
#include <limits>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::MusicalGrid)

namespace Scenario
{
namespace
{

//! Lines closer than this to the line before them are not drawn: they could
//! not be told apart, and zoomed out on many time signature changes there
//! would be one per change.
constexpr double min_line_spacing_px = 5.;

//! Sub-divisions are never finer than this fraction of their bar.
constexpr int64_t max_subdivisions_per_bar = 64;

constexpr int64_t whole_note = 4 * ossia::quarter_duration<int64_t>;

//! Flicks of a bar. Exact: a whole note is 2^10 times an integer.
int64_t barDuration(ossia::time_signature sig) noexcept
{
  return whole_note * sig.upper / std::max<int64_t>(sig.lower, 1);
}

//! Floor of a / b for b > 0.
int64_t floorDiv(int64_t a, int64_t b) noexcept
{
  int64_t q = a / b;
  if((a % b != 0) && (a < 0))
    q--;
  return q;
}

//! For each time signature, the bars counted before it. A bar cut short by
//! the next signature counts as a bar.
void countBarsAtSignatures(
    const TimeSignatureMap& measures, std::vector<int32_t>& bars_at_signature)
{
  bars_at_signature.clear();
  if(measures.empty())
    return;
  bars_at_signature.reserve(measures.size());
  bars_at_signature.push_back(0);

  int32_t bars = 0;
  for(auto it = measures.begin() + 1; it != measures.end(); ++it)
  {
    const auto prev = it - 1;
    const int64_t bar = barDuration(prev->second);
    const int64_t length = it->first.impl - prev->first.impl;
    bars += int32_t((length + bar - 1) / bar);
    bars_at_signature.push_back(bars);
  }
}

ossia::bar_time timeToMetrics(
    const TimeSignatureMap& measures, const std::vector<int32_t>& bars_at_signature,
    TimeVal t)
{
  constexpr int64_t quarter = ossia::quarter_duration<int64_t>;
  ossia::bar_time res{};
  auto sig = ossia::last_before(measures, t);
  const int64_t bar = barDuration(sig->second);
  const int64_t since = t.impl - sig->first.impl;
  const int64_t bars = floorDiv(since, bar);
  const int64_t in_bar = since - bars * bar;

  res.bars = bars_at_signature[std::distance(measures.begin(), sig)] + bars;
  res.quarters = in_bar / quarter;
  const int64_t in_quarter = in_bar - res.quarters * quarter;
  res.semiquavers = in_quarter / (quarter / 4);
  res.cents = (in_quarter - res.semiquavers * (quarter / 4)) / (quarter / 400);
  return res;
}

struct Durations
{
  ossia::bar_time main_bar;
  int64_t main;
};

//! The main lines: every bar, or every power of two bars when zoomed out.
Durations computeDurations(ossia::time_signature sig, double zoom)
{
  const int64_t bar = barDuration(sig);

  const double res = bar / (30. * zoom);
  double pow2 = std::pow(2, std::floor(std::clamp(log2(res), -16., 16.)));

  ossia::bar_time b{};
  if(pow2 >= 1.)
  {
    b.bars = 1;
    return {b, bar};
  }
  else
  {
    b.bars = 1. / pow2;
    return {b, bar * b.bars};
  }
}

//! The sub-divisions of the bars of a signature: its beat halved until they
//! are close enough, never finer than a 64th of the bar.
struct Subdivisions
{
  //! A sub-division is whole_note / per_whole flicks.
  int64_t per_whole{};
  double px{};
};

Subdivisions computeSubdivisions(ossia::time_signature sig, double zoom)
{
  if(sig.upper <= 0 || sig.lower <= 0 || sig.upper > max_subdivisions_per_bar)
    return {};

  int64_t per_beat = 1;
  auto px = [&](int64_t pb) { return whole_note / double(sig.lower * pb) / zoom; };
  while(px(per_beat) > 40. && sig.upper * per_beat * 2 <= max_subdivisions_per_bar)
    per_beat *= 2;

  return {sig.lower * per_beat, px(per_beat)};
}

void computeAll(
    MusicalGrid& grid, TimeSignatureMap::const_iterator sig_it, TimeVal first_line,
    TimeVal timeDelta, double zoom, QRectF rect, double x0_px)
{
  auto& bars = *grid.timebars.lightBars;
  auto& sub = *grid.timebars.lighterBars;
  auto& measures = *grid.m_measures;
  auto& magneticTimings = grid.timebars.magneticTimings;

  const double y0 = rect.y();
  const double y1 = rect.y() + rect.height();

  // Every line is placed from its exact time in flicks, converted once: the
  // sub-divisions of a bar are the same width, on both sides of its lines.
  auto toPx = [zoom](int64_t t) { return t / zoom; };

  // The first line can be up to a division left of the rect: what is visible
  // is measured from the rect's left edge, so that the division at its right
  // end gets its lines too.
  const double view_left_px = x0_px;
  const double view_right_px = x0_px + rect.width();
  double last_line_px = -std::numeric_limits<double>::infinity();
  bool last_line_at_change = false;

  auto addSubdivisions = [&](int64_t from, int64_t to, ossia::time_signature sig) {
    const auto subs = computeSubdivisions(sig, zoom);
    if(subs.per_whole <= 0 || subs.px < min_line_spacing_px)
      return;

    // Zoomed in, a bar holds far more sub-divisions than the view shows:
    // only the ones in the view are made.
    int64_t k = 1;
    const double view_left_t = view_left_px * zoom;
    if(view_left_t > from)
      k = std::max(
          k, int64_t(std::floor((view_left_t - from) * subs.per_whole / whole_note)));

    const double to_px = toPx(to);
    for(;; k++)
    {
      const int64_t t = from + k * whole_note / subs.per_whole;
      if(t >= to)
        break;
      const double kp = toPx(t);
      if(kp > view_right_px)
        break;
      // Where a signature change cuts a bar short, no sub-division right
      // against its line.
      if(to_px - kp < min_line_spacing_px - 0.01)
        break;
      sub.positions.push_back(QLineF(kp, y0, kp, y1));
      magneticTimings.push_back(TimeVal{t} + timeDelta);
    }
  };

  auto addLine = [&](int64_t t, ossia::bar_time increment, bool at_change) {
    const double px = toPx(t);
    QLineF line{px, y0, px, y1};
    MusicalGrid::timings timing{
        px, timeToMetrics(measures, grid.m_barsAtSignature, TimeVal{t} + timeDelta),
        increment};
    if(px - last_line_px >= min_line_spacing_px)
    {
      magneticTimings.push_back(TimeVal{t} + timeDelta);
      bars.positions.push_back(line);
      grid.mainPositions.push_back(timing);
    }
    else if(at_change && !last_line_at_change && !bars.positions.empty())
    {
      // A time signature change is shown over the bar line just before it.
      magneticTimings.push_back(TimeVal{t} + timeDelta);
      bars.positions.back() = line;
      grid.mainPositions.back() = timing;
    }
    else
    {
      return;
    }
    last_line_px = px;
    last_line_at_change = at_change;
  };

  int64_t t = first_line.impl;
  bool has_segment = false;
  int64_t segment_start{};
  ossia::time_signature segment_sig{};
  bool segment_is_bar{};

  for(;;)
  {
    const auto next = sig_it + 1;
    const int64_t sig_start = (sig_it->first - timeDelta).impl;
    const int64_t next_start = next != measures.end()
                                   ? (next->first - timeDelta).impl
                                   : std::numeric_limits<int64_t>::max();
    const auto [increment, division] = computeDurations(sig_it->second, zoom);
    if(division / zoom <= 1.)
      return;

    // The lines of this signature, up to the next one.
    for(; t < next_start; t += division)
    {
      if(has_segment && segment_is_bar)
        addSubdivisions(segment_start, t, segment_sig);

      addLine(t, increment, t == sig_start);

      has_segment = true;
      segment_start = t;
      segment_sig = sig_it->second;
      segment_is_bar = increment.bars == 1;

      // Up to the first line past the right of the view.
      if(toPx(t) >= view_right_px)
        return;
    }

    sig_it = next;
    t = next_start;
  }
}

}

void MusicalGrid::compute(
    TimeVal timeDelta, ZoomRatio zoom, QRectF sceneRect, TimeVal x0_time)
{
  SCORE_ASSERT(m_measures);
  auto& measures = *m_measures;

  // Scrolling in the rest of the view, or moving something in the interval,
  // asks for the grid again with nothing it depends on changed.
  const bool same_measures = m_computed && !(m_computedMeasures != measures);
  if(same_measures && m_computedTimeDelta == timeDelta && m_computedZoom == zoom
     && m_computedRect == sceneRect && m_computedX0 == x0_time)
    return;

  if(!same_measures)
  {
    m_computedMeasures = measures;
    countBarsAtSignatures(measures, m_barsAtSignature);
  }
  m_computed = true;
  m_computedTimeDelta = timeDelta;
  m_computedZoom = zoom;
  m_computedRect = sceneRect;
  m_computedX0 = x0_time;

  mainPositions.clear();
  //subPositions.clear();
  timebars.lightBars->positions.clear();
  timebars.lighterBars->positions.clear();
  timebars.magneticTimings.clear();

  // Find the first measure we see
  auto last_signature_change = ossia::last_before(measures, x0_time);
  if(last_signature_change == measures.end())
    return;

  const auto [inc, main_div_source]
      = computeDurations(last_signature_change->second, zoom);

  // The main line at or before the left of the view.
  const int64_t q
      = floorDiv((x0_time - last_signature_change->first).impl, main_div_source);
  const int64_t first_main_delim_local
      = last_signature_change->first.impl + q * main_div_source;

  // Where we start counting from
  const TimeVal first_main_delim = TimeVal(first_main_delim_local - timeDelta.impl);

  const double x0_px = (x0_time - timeDelta).toPixelsRaw(zoom);
  computeAll(
      *this, last_signature_change, first_main_delim, timeDelta, zoom, sceneRect, x0_px);

  ossia::remove_duplicates(timebars.magneticTimings);
  //, [] (auto t1, auto t2) { return t1 < t2});

  // To the ruler, whose 0 is the leftmost pixel shown. The positions are in
  // the interval shown; x0_time is in the one with the time signatures.
  for(auto& [v, _1, _2] : mainPositions)
    v -= x0_px + 100;
  // for (auto& [v, _1, _2] : subPositions)
  //   v -= x0_time.toPixels(zoom) + 100;
  /*
  {
    qDebug(" =================== ");
    for(int i = 0; i < std::min(std::ssize(mainPositions), 3); i++)
      qDebug() << "Main " << i <<  mainPositions[i];
    //for(int i = 0; i < std::min(std::ssize(subPositions), 8); i++)
    //  qDebug() << "Sub" << i <<  subPositions[i];
    for(int i = 0; i < std::min((int)timebars.magneticTimings.size(), 8); i++)
      qDebug() << "Magnetic" << i <<  timebars.magneticTimings[i].toQTime();
  }
*/
  this->timebars.lightBars->updateShapes();
  this->timebars.lighterBars->updateShapes();
  changed();
}

}
