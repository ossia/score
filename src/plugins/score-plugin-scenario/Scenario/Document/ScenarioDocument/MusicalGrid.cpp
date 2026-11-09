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

//! For each time signature, the bars counted before it.
void countBarsAtSignatures(
    const TimeSignatureMap& measures, std::vector<int32_t>& bars_at_signature)
{
  bars_at_signature.clear();
  if(measures.empty())
    return;
  bars_at_signature.reserve(measures.size());
  bars_at_signature.push_back(0);

  int32_t start_bars = 0;
  int64_t prev_bar_date = 0;
  ossia::time_signature prev_sig = measures.begin()->second;

  for(auto it = measures.begin() + 1; it != measures.end(); ++it)
  {
    const auto sig_upper = prev_sig.upper;
    const auto sig_lower = prev_sig.lower;
    int64_t this_bar_date = it->first.impl;
    auto [quarters, qrem]
        = std::div((this_bar_date - prev_bar_date), ossia::quarter_duration<int64_t>);
    auto [bars, brem] = std::div(quarters, (4. * double(sig_upper) / sig_lower));

    start_bars += bars;
    // If the signature change falls on "not a previous bar",
    // we increment the bar counter
    if(qrem != 0 || brem != 0)
    {
      start_bars += 1;
    }

    if(bars * (4. * double(sig_upper) / sig_lower))
      prev_bar_date = this_bar_date;
    prev_sig = it->second;
    bars_at_signature.push_back(start_bars);
  }
}

ossia::bar_time timeToMetrics(
    const TimeSignatureMap& measures, const std::vector<int32_t>& bars_at_signature,
    TimeVal x0_time)
{
  ossia::bar_time start{};
  auto last_before = ossia::last_before(measures, x0_time);
  start.bars = bars_at_signature[std::distance(measures.begin(), last_before)];
  {
    const auto sig_upper = last_before->second.upper;
    const auto sig_lower = last_before->second.lower;
    int64_t flicks_since_last_sig = (x0_time.impl - last_before->first.impl);
    int64_t quarters_since_last_sig
        = flicks_since_last_sig / ossia::quarter_duration<int64_t>;
    int64_t bars = quarters_since_last_sig / (4. * double(sig_upper) / sig_lower);
    start.bars += bars;
    start.quarters
        = quarters_since_last_sig - bars * (4. * double(sig_upper) / sig_lower);
    start.semiquavers = (flicks_since_last_sig
                         - quarters_since_last_sig * ossia::quarter_duration<int64_t>)
                        / (ossia::quarter_duration<int64_t> / 4);
    start.cents
        = (flicks_since_last_sig
           - quarters_since_last_sig
                 * ossia::quarter_duration<
                     int64_t> - start.semiquavers * (ossia::quarter_duration<int64_t> / 4))
          / (ossia::quarter_duration<int64_t> / 400);
  }
  return start;
}

struct Durations
{
  double pow2;
  ossia::bar_time main_bar;
  int64_t main;
};

Durations computeDurations(ossia::time_signature sig, double zoom)
{
  // Duration of a quarter note
  constexpr const int64_t quarter = ossia::quarter_duration<int64_t>;

  // Duration of a bar
  const int64_t whole = quarter * (4. * double(sig.upper) / sig.lower);

  const double res = whole / (30. * zoom);
  double pow2
      = /*std::floor*/ (std::pow(2, std::floor(std::clamp(log2(res), -16., 16.))));

  ossia::bar_time b{};
  int64_t main_div_source = whole;

  //if (pow2_floor >= 1. && pow2_floor <= 3.) // between bars and 8th notes
  if(pow2 >= 1.)
  {
    // Main unit is the bar.
    pow2 = 1.;
    b.bars = 1;
  }
  else
  {
    b.bars = 1. / pow2; // pow2 is always > 0
  }

  return {pow2, b, int64_t(main_div_source / pow2)};
  /*
  else if (pow2_floor > 3 && pow2_floor <= 8) // between 16th and 32th notes
  {
    // Main unit is the quarter note.
    pow2 = 4. * double(sig.upper) / sig.lower;
    b.quarters = 1;

    return {pow2, b, int64_t(quarter)};
  }
  else if (pow2_floor > 8 && pow2_floor < 12) // between 16th and ..th notes
  {
    pow2 = 4. * 4. * double(sig.upper) / sig.lower;
    b.semiquavers = 1;

    // Main unit is the 16th note.
    return {pow2, b, int64_t(quarter / 4)};
  }
  else if (pow2 < 1.)
  {
    b.bars = (double(sig.upper) / sig.lower);
  }
  else
  {
    pow2 = 8. * 4. * double(sig.upper) / sig.lower;
    b.cents = 1;
  }

  qDebug() << pow2 << "[" << b.bars << b.quarters << b.semiquavers << b.cents << "]" << main_div_source;

  //pow2 /= (double(sig.upper) / sig.lower);

  return {pow2, b, int64_t(main_div_source / pow2)};
  */
}
/*
void addBars(ossia::bar_time& time, ossia::bar_time& increment)
{
  time.bars += increment.bars;
  time.quarters += increment.quarters;
  time.semiquavers += increment.semiquavers;
  time.cents += increment.cents;
}
*/

void computeAll(
    MusicalGrid& grid, TimeSignatureMap::const_iterator last_sig_change_it,
    TimeVal last_delim, TimeVal division, TimeVal timeDelta, double zoom,
    ossia::bar_time increment, QRectF rect, double x0_px)
{
  //qDebug("\n\n\n starting \n");
  auto& bars = *grid.timebars.lightBars;
  auto& sub = *grid.timebars.lighterBars;
  auto& measures = *grid.m_measures;
  auto& magneticTimings = grid.timebars.magneticTimings;

  const double y0 = rect.y();
  const double y1 = rect.y() + rect.height();

  double division_px = division.impl / zoom;
  if(division_px <= 1.)
    return;

  TimeVal prev_time = last_delim;
  TimeVal current_time = last_delim;
  double last_delim_px = last_delim.toPixels(zoom);
  double bar_x_pos{last_delim_px};
  double prev_bar_x_pos{last_delim_px};

  ossia::time_signature prev_sig = last_sig_change_it->second;

  TimeVal sub_increment_t{};
  double sub_increment_px{};
  sub_increment_t = TimeVal(ossia::quarter_duration<double>);
  sub_increment_px = sub_increment_t.toPixels(zoom);

  while(sub_increment_px > 40)
  {
    sub_increment_t.impl /= 2.;
    sub_increment_px = sub_increment_t.toPixels(zoom);
  }
  // The first line can be up to a division left of the rect: what is visible
  // is measured from the rect's left edge, so that the division at its right
  // end gets its lines too.
  const double view_left_px = x0_px;
  const double view_right_px = x0_px + rect.width();
  auto isVisible = [&](double bar_x_pos) { return bar_x_pos < view_right_px; };
  double last_line_px = -std::numeric_limits<double>::infinity();

  // This function adds a main vertical line, and adds
  // the sub-bars between the new main and the previous one.
  auto addNewMain = [&](double new_bar_x_pos, TimeVal prev_t, TimeVal cur_t) {
    if(new_bar_x_pos - last_line_px >= min_line_spacing_px)
    {
      bars.positions.push_back(QLineF(new_bar_x_pos, y0, new_bar_x_pos, y1));
      // Lines are drawn in the interval shown, bars counted in the one that
      // holds the time signatures.
      grid.mainPositions.push_back(
          {new_bar_x_pos,
           timeToMetrics(measures, grid.m_barsAtSignature, cur_t + timeDelta),
           increment});
      magneticTimings.push_back(cur_t + timeDelta);
      last_line_px = new_bar_x_pos;
    }

    if(increment.bars != 1)
    {
      /*
      // Just cut things in half
      sub_increment_t = (cur_t.impl - prev_t.impl) / 2;//TimeVal(zoom * (bar_x_pos - prev_bar_x_pos) / 2.);
      sub_increment_px = (new_bar_x_pos - prev_bar_x_pos) / 2.;
      */
      return;
    }

    // We display the quarter notes
    // Only display sub bars if there is enough visual space.
    if(sub_increment_px >= 5. && sub_increment_t.impl > 0)
    {
      // auto sub_time = main_time;
      // addBars(sub_time, sub_increment);

      // Zoomed in, a bar holds far more sub-divisions than the view shows:
      // only the ones in the view are made.
      int64_t first = 1;
      const double view_left_t = view_left_px * zoom;
      if(view_left_t > prev_t.impl)
        first = std::max(
            first,
            int64_t(std::floor((view_left_t - prev_t.impl) / sub_increment_t.impl)));

      TimeVal sub_bar_t{prev_t.impl + sub_increment_t.impl * first};
      while(sub_bar_t < cur_t)
      {
        double kp = sub_bar_t.toPixels(zoom);
        if(kp > view_right_px)
          break;
        sub.positions.push_back(QLineF(kp, y0, kp, y1));
        magneticTimings.push_back(sub_bar_t + timeDelta);
        sub_bar_t += sub_increment_t;
      }
      /*
      for (int sub_k = 0; pos <= new_bar_x_pos; sub_k++)
      {
        auto kt = TimeVal(prev_t.impl + sub_increment_t.impl * sub_k);
        double kp = kt.toPixels(zoom);
        qDebug() << "      -> sub bar" << pos;
        qDebug() << "      -> magnetic timing" << kt;
        sub.positions.push_back(QLineF(kp, y0, kp, y1));
        magneticTimings.push_back(kt);
        pos = prev_bar_x_pos + sub_k * sub_increment_px;
        // grid.subPositions.push_back({pos, timeToMetrics(grid, cur_t),
        // sub_increment}); addBars(sub_time, sub_increment);
      }
      */
    }

    prev_sig = last_sig_change_it->second;
    prev_bar_x_pos = new_bar_x_pos;
    // addBars(main_time, increment);
  };

  int k = 0;
  while(isVisible(bar_x_pos - division_px))
  {
    bar_x_pos = last_delim_px + k * division_px;
    prev_time = current_time;
    current_time = last_delim.impl + division.impl * k;
    SCORE_ASSERT(last_sig_change_it != measures.end());

    auto next = last_sig_change_it + 1;
    if(next == measures.end())
    {
      // We're displaying the bars that are after the last entry
      // in the time signature map - everything falls in this case
      // if there is e.g. only a default 4/4 at the beginning
      do
      {
        addNewMain(bar_x_pos, prev_time, current_time);

        k++;
        bar_x_pos = last_delim_px + k * division_px;
        prev_time = current_time;
        current_time = last_delim.impl + division.impl * k;
      } while(isVisible(bar_x_pos - division_px));

      return;
    }
    else if(bar_x_pos >= (next->first - timeDelta).toPixels(zoom))
    {
      // Time signature change
      last_sig_change_it = next;

      auto [p2, inc, main_division] = computeDurations(last_sig_change_it->second, zoom);
      increment = inc;
      division = main_division;
      division_px = main_division / zoom;

      last_delim = (last_sig_change_it->first - timeDelta);
      last_delim_px = last_delim.toPixels(zoom);

      addNewMain(last_delim_px, prev_time, last_delim);

      k = 0;
    }
    else
    {
      // Default case between two measure changes
      // TODO merge it with the one above

      // note: if a measure change falls on a new measure
      // we get a duplicated bar here
      addNewMain(bar_x_pos, prev_time, current_time);
      k++;
    }
  }
}
/*
ossia::bar_time computeStart(MusicalGrid& grid, TimeVal x0_time,
TimeSignatureMap::const_iterator last_before)
{
  // Compute the amount of bars before so that we can index them
  ossia::bar_time start{};
  if(last_before != grid.measures.begin())
  {
    int64_t prev_bar_date = 0;
    ossia::time_signature prev_sig = grid.measures.begin()->second;

    auto it = grid.measures.begin() + 1;
    for (; it <= last_before; ++it)
    {
      const auto sig_upper = prev_sig.upper;
      const auto sig_lower = prev_sig.lower;
      int64_t this_bar_date = it->first.impl;
      int32_t quarters = (this_bar_date - prev_bar_date) /
ossia::quarter_duration<int64_t>; int32_t bars = quarters / (4. *
double(sig_upper) / sig_lower);

      start.bars += bars;

      prev_bar_date = this_bar_date;
      prev_sig = it->second;
    }
  }
  {
    const auto sig_upper = last_before->second.upper;
    const auto sig_lower = last_before->second.lower;
    int32_t quarters = (x0_time.impl - last_before->first.impl) /
ossia::quarter_duration<int64_t>; int32_t bars = quarters / (4. *
double(sig_upper) / sig_lower); start.bars += bars; start.quarters = quarters -
bars * (4. * double(sig_upper) / sig_lower);
  }
  return start;
}
*/
}
/*
static
QDebug operator<<(QDebug d, MusicalGrid::timings t)
{
  d << QString("%1  inc: [%2, %3, %4, %5]  t: [%6, %7, %8, %9]")
       .arg(t.pos_x)
       .arg(t.increment.bars)
       .arg(t.increment.quarters)
       .arg(t.increment.semiquavers)
       .arg(t.increment.cents)
       .arg(t.timings.bars)
       .arg(t.timings.quarters)
       .arg(t.timings.semiquavers)
       .arg(t.timings.cents);
  return d;
}
*/
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

  const auto [pow2, inc, main_div_source]
      = computeDurations(last_signature_change->second, zoom);

  const TimeVal main_division = TimeVal(main_div_source);

  const int64_t q
      = std::floor((x0_time - last_signature_change->first).impl / main_div_source);
  const int64_t first_main_delim_local
      = last_signature_change->first.impl + q * main_div_source;

  // Where we start counting from
  const TimeVal first_main_delim = TimeVal(first_main_delim_local - timeDelta.impl);

  computeAll(
      *this, last_signature_change, first_main_delim, main_division, timeDelta, zoom,
      inc, sceneRect, (x0_time - timeDelta).toPixels(zoom));

  ossia::remove_duplicates(timebars.magneticTimings);
  //, [] (auto t1, auto t2) { return t1 < t2});

  // To the ruler, whose 0 is the leftmost pixel shown. The positions are in
  // the interval shown; x0_time is in the one with the time signatures.
  for(auto& [v, _1, _2] : mainPositions)
    v -= (x0_time - timeDelta).toPixels(zoom) + 100;
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
