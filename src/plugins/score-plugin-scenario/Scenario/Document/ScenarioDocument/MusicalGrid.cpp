#include "MusicalGrid.hpp"
#include <Scenario/Document/Interval/FullView/TimeSignatureItem.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>
#include <QLineF>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::MusicalGrid)

namespace Scenario
{
namespace
{

ossia::bar_time timeToMetrics(MusicalGrid& grid, TimeVal x0_time)
{
  // Compute the amount of bars before so that we can index them
  ossia::bar_time start{};
  auto last_before = ossia::last_before(grid.measures, x0_time);

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
      int32_t quarters = (this_bar_date - prev_bar_date) / ossia::quarter_duration<int64_t>;
      int32_t bars = quarters / (4. * double(sig_upper) / sig_lower);

      start.bars += bars + 1;

      prev_bar_date = this_bar_date;
      prev_sig = it->second;
    }
  }
  {
    const auto sig_upper = last_before->second.upper;
    const auto sig_lower = last_before->second.lower;
    int64_t flicks_since_last_signature = (x0_time.impl - last_before->first.impl);
    int64_t quarters = flicks_since_last_signature / ossia::quarter_duration<int64_t>;
    int64_t bars = quarters / (4. * double(sig_upper) / sig_lower);
    start.bars += bars;
    start.quarters = quarters - bars * (4. * double(sig_upper) / sig_lower);
    start.semiquavers = (flicks_since_last_signature - quarters * ossia::quarter_duration<int64_t>) / (ossia::quarter_duration<int64_t> / 4);
    start.cents = (flicks_since_last_signature - quarters * ossia::quarter_duration<int64_t> - start.semiquavers * (ossia::quarter_duration<int64_t> / 4)) / (ossia::quarter_duration<int64_t> / 400);
  }
  return start;
}


struct Durations {
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
  double pow2 = std::pow(2, std::floor(std::clamp(log2(res), -16., 16.)));

  ossia::bar_time b{};
  int64_t main_div_source = whole;

  if(pow2 >= 1. && pow2 <= 3.) // between bars and 8th notes
  {
    pow2 = 1.;
    b.bars = 1;
  }
  else if(pow2 > 3 && pow2 <= 8) // between 16th and 32th notes
  {
    pow2 = 4.;
    b.quarters = 1;
  }
  else if(pow2 > 8 && pow2 < 12) // between 16th and ..th notes
  {
    pow2 = 10.;
    b.semiquavers = 1;
  }
  else if(pow2 < 1.)
  {
    b.bars = 1. / pow2;
  }
  else
  {
    if(pow2 > 14) pow2 = 14;
    b.cents = 1;
  }

  pow2 /= (double(sig.upper) / sig.lower);


  return {pow2, b, int64_t(main_div_source / pow2)};
}

void addBars(ossia::bar_time& time, ossia::bar_time& increment)
{
  time.bars += increment.bars;
  time.quarters += increment.quarters;
  time.semiquavers += increment.semiquavers;
  time.cents += increment.cents;
}

void computeAll(
    MusicalGrid& grid,
    TimeSignatureMap::const_iterator last_sig_change_it,
    TimeVal last_delim,
    TimeVal division,
    TimeVal timeDelta,
    double zoom,
    ossia::bar_time increment,
    QRectF rect)
{
  auto& bars = grid.timebars.lightBars;
  auto& sub = grid.timebars.lighterBars;
  auto& measures = grid.measures;
  auto& magneticTimings = grid.timebars.magneticTimings;

  int k = 0;
  const double y0 = rect.y();
  const double y1 = rect.y() + rect.height();

  double division_px = division.impl / zoom;
  if(division_px <= 1.)
    return;

  TimeVal current_time = last_delim;
  double last_delim_px = last_delim.toPixels(zoom);
  double bar_x_pos{last_delim_px};
  double prev_bar_x_pos{last_delim_px};

  ossia::time_signature prev_sig = last_sig_change_it->second;

  //ossia::bar_time main_time = grid.start;
  auto isVisible = [&] (double bar_x_pos) {
    return (bar_x_pos - last_delim_px) < rect.width();
  };
  // This function adds a main vertical line, and adds
  // the sub-bars between the new main and the previous one.
  auto addNewMain = [&] (double bar_x_pos, TimeVal cur_t) {
    bars.positions.push_back(QLineF(bar_x_pos, y0, bar_x_pos, y1));
    grid.mainPositions.push_back({bar_x_pos, timeToMetrics(grid, cur_t), increment});
    magneticTimings.push_back(cur_t);

    ossia::bar_time sub_increment{};
    TimeVal sub_increment_t{};
    double sub_increment_px{};
    if(increment.bars == 1)
    {
      // We display the quarter notes
      sub_increment.quarters = 1;
      sub_increment_t = TimeVal(ossia::quarter_duration<int64_t> * 4. / prev_sig.lower);
      sub_increment_px = sub_increment_t.toPixels(zoom);
    }
    else
    {
      // Just cut things in half
      sub_increment_t = TimeVal(zoom * (bar_x_pos - prev_bar_x_pos) / 2.);
      sub_increment_px = (bar_x_pos - prev_bar_x_pos) / 2.;
    }

    // Only display sub bars if there is enough visual space.
    if(sub_increment_px >= 5.)
    {
      //auto sub_time = main_time;
      //addBars(sub_time, sub_increment);
      int sub_k = 1;
      double pos = prev_bar_x_pos + sub_increment_px;
      for(; pos < bar_x_pos; pos += sub_increment_px)
      {
        sub.positions.push_back(QLineF(pos, y0, pos, y1));
        magneticTimings.push_back(TimeVal(cur_t.impl + sub_increment_t.impl * (sub_k++)));
        //grid.subPositions.push_back({pos, timeToMetrics(grid, cur_t), sub_increment});
        //addBars(sub_time, sub_increment);
      }
    }

    prev_sig = last_sig_change_it->second;
    prev_bar_x_pos = bar_x_pos;
    //addBars(main_time, increment);
  };

  while(isVisible(bar_x_pos))
  {
    bar_x_pos = last_delim_px + k * division_px;
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
        addNewMain(bar_x_pos,current_time);

        k++;
        bar_x_pos = last_delim_px + k * division_px;
        current_time = last_delim.impl + division.impl * k;
      } while(isVisible(bar_x_pos));

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

      addNewMain(last_delim_px, last_delim);

      k = 0;
    }
    else
    {
      // Default case between two measure changes
      // TODO merge it with the one above
      addNewMain(bar_x_pos, current_time);
      k++;
    }
  }
}
/*
ossia::bar_time computeStart(MusicalGrid& grid, TimeVal x0_time, TimeSignatureMap::const_iterator last_before)
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
      int32_t quarters = (this_bar_date - prev_bar_date) / ossia::quarter_duration<int64_t>;
      int32_t bars = quarters / (4. * double(sig_upper) / sig_lower);

      start.bars += bars;

      prev_bar_date = this_bar_date;
      prev_sig = it->second;
    }
  }
  {
    const auto sig_upper = last_before->second.upper;
    const auto sig_lower = last_before->second.lower;
    int32_t quarters = (x0_time.impl - last_before->first.impl) / ossia::quarter_duration<int64_t>;
    int32_t bars = quarters / (4. * double(sig_upper) / sig_lower);
    start.bars += bars;
    start.quarters = quarters - bars * (4. * double(sig_upper) / sig_lower);
  }
  return start;
}
*/
}

void MusicalGrid::compute(
  TimeVal timeDelta,
  ZoomRatio zoom,
  QRectF sceneRect,
  TimeVal x0_time)
{
  // Find the last measure change before x0_time.
  // Find the first measure we see
  auto last_signature_change = ossia::last_before(measures, x0_time);

  //this->start = computeStart(*this, x0_time, last_signature_change);

  // TODO compute instead everything in time_values, store them, and then map those to pixels

  mainPositions.clear();
  subPositions.clear();
  timebars.lightBars.positions.clear();
  timebars.lighterBars.positions.clear();
  timebars.magneticTimings.clear();

  // We want to find the subdivision that will result in the smallest bars being at least pixels_width_min pixels.
  // We have to solve : main_division_pixels > pixels_width_min.
  // This gives : subdivision < whole_duration / (zoom_ratio * pixels_width_min)

  const auto [pow2, inc, main_div_source] = computeDurations(last_signature_change->second, zoom);

  const TimeVal main_division = TimeVal(main_div_source);

  const int64_t q = std::floor((x0_time - last_signature_change->first).impl / main_div_source);
  const int64_t first_main_delim_local = last_signature_change->first.impl + q * main_div_source;

  // Where we start counting from
  const TimeVal first_main_delim = TimeVal(first_main_delim_local - timeDelta.impl);

  computeAll(*this, last_signature_change, first_main_delim, main_division, timeDelta, zoom, inc, sceneRect);

  for(auto& [v, _1, _2] : mainPositions) v -= x0_time.toPixels(zoom) + 100;
  for(auto& [v, _1, _2] : subPositions) v -= x0_time.toPixels(zoom) + 100;

  this->timebars.lightBars.updateShapes();
  this->timebars.lighterBars.updateShapes();
  changed();
}


}
