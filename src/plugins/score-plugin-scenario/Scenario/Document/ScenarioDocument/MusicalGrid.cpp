#include "MusicalGrid.hpp"
#include <Scenario/Document/Interval/FullView/TimeSignatureItem.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>
#include <QLineF>

namespace Scenario
{

namespace
{

struct Durations {
  double pow2;
  int64_t main;
  int64_t sub;
};
Durations computeDurations(ossia::time_signature sig, double zoom)
{
  // We don't want things smaller than that
  constexpr const double pixels_width_min = 30.;

  // Duration of a quarter note in flicks
  constexpr const int64_t quarter = ossia::quarter_duration<int64_t>;

  // Duration of a bar in milliseconds.
  const int64_t whole = quarter * (4. * double(sig.upper) / sig.lower);

  qDebug() << whole << sig.upper << sig.lower;
  const double res = whole / (pixels_width_min * zoom);
  double pow2 = std::pow(2, std::floor(std::clamp(log2(res), -16., 16.)));

  int64_t main_div_source{};
  int64_t sub_div_source{};

  main_div_source = whole;
  sub_div_source = quarter;
  if(pow2 >= 1. && pow2 <= 3.) // between bars and 8th notes
  {
    //qDebug() << "case 1";
    //main_div_source = whole;
    //sub_div_source = quarter;
    pow2 = 1.;

  }
  else if(pow2 > 3 && pow2 <= 8) // between 16th and 32th notes
  {
    //main_div_source = whole; // main is quarter notes
    //sub_div_source = quarter / 2;
    pow2 = 4.;
  }
  else
  {
    //qDebug() << "case 3";
    // Else we just divide by 2
    // -> ratio of 2 between main and sub
    //main_div_source = whole;
    sub_div_source = whole / 2.;
  }

  pow2 /= (double(sig.upper) / sig.lower);;
  return {pow2, main_div_source, sub_div_source};
}

void computeMain(
    MusicalGrid& grid,
    TimeSignatureMap::const_iterator last_before,
    double zoom,
    double last_quarter_pixels,
    double division_pixels,
    double pow2,
    double delta_pixels,
    QRectF sceneRect)
{
  double pixelRatio = 1. / (pow2 * zoom);
  auto& bars = grid.timebars->lightBars;
  auto& sub = grid.timebars->lighterBars;
  auto& measures = grid.measures;

  int i = 0;
  int k = 0;
  const double y0 = sceneRect.y();
  const double y1 = sceneRect.y() + sceneRect.height();

  double bar_x_pos{};
  while((bar_x_pos - last_quarter_pixels) < sceneRect.width())
  {
    bar_x_pos = last_quarter_pixels + k * division_pixels;
    SCORE_ASSERT(last_before != measures.end());

    auto next = last_before + 1;
    if(next == measures.end())
    {
      // We're displaying the bars that are after the last entry
      // in the time signature map - everything falls in this case
      // if there is e.g. only a default 4/4 at the beginning
      bars[i] = QLineF(bar_x_pos, y0, bar_x_pos, y1);
      grid.mainPositions.push_back({bar_x_pos, {}});
      i++;
      k++;
    }
    else if(bar_x_pos >= (next->first.toPixels(zoom) - delta_pixels))
    {
      // Measure change !
      last_before = next;

      auto [p2, main_div_source, sub_div_source] = computeDurations(last_before->second, zoom);
      pow2 = p2;
      pixelRatio = 1. / (pow2 * zoom);
      /*
      const auto sig_upper = last_before->second.upper;
      const auto sig_lower = last_before->second.lower;

      // Duration of a quarter note
      constexpr const int64_t quarter = ossia::quarter_duration<int64_t>;

      // Duration of a bar
      const int64_t whole = quarter * (4. * double(sig_upper) / sig_lower);
      const auto main_div_source = whole;

      double main_div_source{};

      if(pow2 >= 1. && pow2 <= 2.) // between bars and 8th notes
      {
        main_div_source = whole * pow2;
      }
      else if(pow2 > 2. && pow2 <= 8.) // between 16th and 32th notes
      {
        main_div_source = quarter * pow2; // main is quarter notes
      }
      else
      {
        // Else we just divide by 2
        // -> ratio of 2 between main and sub
        main_div_source = whole;
      }
*/
      division_pixels = main_div_source * pixelRatio;
          qDebug( ) << division_pixels;

      last_quarter_pixels = last_before->first.toPixels(zoom) - delta_pixels;

      bars[i] = QLineF(last_quarter_pixels, y0, last_quarter_pixels, y1);
      grid.mainPositions.push_back({last_quarter_pixels, {}});

      i++;
      k = 0;
    }
    else
    {
      // Default case between two measure changes
      // TODO merge it with the one above
      bars[i] = QLineF(bar_x_pos, y0, bar_x_pos, y1);
      grid.mainPositions.push_back({bar_x_pos, {}});
      i++;
      k++;
    }
  }

  if(i > 0)
    bars.positions.resize(i - 1);
}

void computeSub(
    MusicalGrid& grid,
    TimeSignatureMap::const_iterator last_before,
    double zoom,
    double last_quarter_pixels,
    double division_pixels,
    double delta_pixels,
    QRectF sceneRect)
{
  auto& bars = grid.timebars->lighterBars;
  auto& measures = grid.measures;

  int i = 0;
  int k = 0;
  const double y0 = sceneRect.y();
  const double y1 = sceneRect.y() + sceneRect.height();

  double bar_x_pos{};
  while((bar_x_pos - last_quarter_pixels) < sceneRect.width())
  {
    bar_x_pos = last_quarter_pixels + k * division_pixels;
    SCORE_ASSERT(last_before != measures.end());

    auto next = last_before + 1;
    if(next == measures.end())
    {
      grid.subPositions.push_back({bar_x_pos, {}});
      bars[i] = QLineF(bar_x_pos, y0, bar_x_pos, y1);
      i++;
      k++;
    }
    else if(bar_x_pos >= (next->first.toPixels(zoom) - delta_pixels))
    {
      last_before = next;

      last_quarter_pixels = last_before->first.toPixels(zoom) - delta_pixels;

      bars[i] = QLineF(last_quarter_pixels, y0, last_quarter_pixels, y1);
      grid.subPositions.push_back({last_quarter_pixels, {}});

      i++;
      k = 0;
    }
    else
    {
      bars[i] = QLineF(bar_x_pos, y0, bar_x_pos, y1);
      grid.subPositions.push_back({bar_x_pos, {}});
      i++;
      k++;
    }

  }
  if(i > 0)
    bars.positions.resize(i - 1);
}


void computeAll(
    MusicalGrid& grid,
    TimeSignatureMap::const_iterator last_before,
    double zoom,
    double last_quarter_pixels,
    double division_pixels,
    double pow2,
    double delta_pixels,
    QRectF sceneRect)
{
  double pixelRatio = 1. / (pow2 * zoom);
  auto& bars = grid.timebars->lightBars;
  auto& sub = grid.timebars->lighterBars;
  auto& measures = grid.measures;

  int i = 0;
  int k = 0;
  const double y0 = sceneRect.y();
  const double y1 = sceneRect.y() + sceneRect.height();

  double bar_x_pos{};
  while((bar_x_pos - last_quarter_pixels) < sceneRect.width())
  {
    bar_x_pos = last_quarter_pixels + k * division_pixels;
    SCORE_ASSERT(last_before != measures.end());

    auto next = last_before + 1;
    if(next == measures.end())
    {
      // We're displaying the bars that are after the last entry
      // in the time signature map - everything falls in this case
      // if there is e.g. only a default 4/4 at the beginning
      bars[i] = QLineF(bar_x_pos, y0, bar_x_pos, y1);
      grid.mainPositions.push_back({bar_x_pos, {}});
      i++;
      k++;
    }
    else if(bar_x_pos >= (next->first.toPixels(zoom) - delta_pixels))
    {
      // Measure change !
      last_before = next;

      auto [p2, main_div_source, sub_div_source] = computeDurations(last_before->second, zoom);
      pow2 = p2;
      pixelRatio = 1. / (pow2 * zoom);
      /*
      const auto sig_upper = last_before->second.upper;
      const auto sig_lower = last_before->second.lower;

      // Duration of a quarter note
      constexpr const int64_t quarter = ossia::quarter_duration<int64_t>;

      // Duration of a bar
      const int64_t whole = quarter * (4. * double(sig_upper) / sig_lower);
      const auto main_div_source = whole;

      double main_div_source{};

      if(pow2 >= 1. && pow2 <= 2.) // between bars and 8th notes
      {
        main_div_source = whole * pow2;
      }
      else if(pow2 > 2. && pow2 <= 8.) // between 16th and 32th notes
      {
        main_div_source = quarter * pow2; // main is quarter notes
      }
      else
      {
        // Else we just divide by 2
        // -> ratio of 2 between main and sub
        main_div_source = whole;
      }
*/
      division_pixels = main_div_source * pixelRatio;
          qDebug( ) << division_pixels;

      last_quarter_pixels = last_before->first.toPixels(zoom) - delta_pixels;

      bars[i] = QLineF(last_quarter_pixels, y0, last_quarter_pixels, y1);
      grid.mainPositions.push_back({last_quarter_pixels, {}});

      i++;
      k = 0;
    }
    else
    {
      // Default case between two measure changes
      // TODO merge it with the one above
      bars[i] = QLineF(bar_x_pos, y0, bar_x_pos, y1);
      grid.mainPositions.push_back({bar_x_pos, {}});
      i++;
      k++;
    }
  }

  if(i > 0)
    bars.positions.resize(i - 1);
}

}
void MusicalGrid::compute(
    TimeVal timeDelta,
    ZoomRatio zoom,
    QRectF sceneRect,
    TimeVal x0_time)
{
  // TODO compute instead everything in time_values, store them, and then map those to pixels
  /*
  mainPositions.clear();
  subPositions.clear();

  // Find the last measure change before x0_time.
  // Find the first measure we see
  auto last_before = ossia::last_before(measures, x0_time);

  // Compute the amount of bars before so that we can index them
  startBar = 0;
  if(last_before != measures.begin())
  {
    int64_t prev_bar_date = 0;
    auto it = measures.begin() + 1;
    for (; it != last_before; ++it)
    {
      const auto sig_upper = it->second.upper;
      const auto sig_lower = it->second.lower;
      int64_t this_bar_date = it->first.impl;
      int32_t quarters = (this_bar_date - prev_bar_date) / ossia::quarter_duration<int64_t>;
      int32_t bars =  quarters / (4. * double(sig_upper) / sig_lower);

      startBar += bars;

      prev_bar_date = this_bar_date;
    }
  }
  {
    const auto sig_upper = last_before->second.upper;
    const auto sig_lower = last_before->second.lower;
    int32_t quarters = (x0_time.impl - last_before->first.impl) / ossia::quarter_duration<int64_t>;
    int32_t bars = quarters / (4. * double(sig_upper) / sig_lower);
    startBar += bars;
  }

  // We want to find the subdivision that will result in the smallest bars being at least pixels_width_min pixels.
  // We have to solve : main_division_pixels > pixels_width_min.
  // This gives : subdivision < whole_duration / (zoom_ratio * pixels_width_min)


  auto [pow2, main_div_source, sub_div_source] = computeDurations(last_before->second, zoom);

  const double deltaPixels = timeDelta.toPixels(zoom);
  const double pixelRatio = 1. / (pow2 * zoom);

  const TimeVal main_division = TimeVal(main_div_source / pow2);
  const double main_division_pixels = main_div_source * pixelRatio;

  const TimeVal sub_division = TimeVal(sub_div_source / pow2);
  const double sub_division_pixels = sub_div_source * pixelRatio;

  const double q = std::floor((x0_time - last_before->first).impl / main_division.impl);
  const double last_quarter_before = last_before->first.impl + q * main_division.impl;

  // Where we start counting from
  double last_quarter_pixels = TimeVal(last_quarter_before).toPixels(zoom) - deltaPixels;

  magneticDivision = sub_division;

  SCORE_ASSERT(main_division_pixels > 0);
  SCORE_ASSERT(sub_division_pixels > 0);

  //computeSub(*this, last_before, zoom, last_quarter_pixels, sub_division_pixels, deltaPixels, sceneRect);
  computeMain(*this, last_before, zoom, last_quarter_pixels, main_division_pixels, pow2, deltaPixels, sceneRect);

  const double x0_px = x0_time.toPixels(zoom) - last_before->first.toPixels(zoom) + 10;
  for(auto& [v, _] : mainPositions) v -= x0_px;
  for(auto& [v, _] : subPositions) v -= x0_px;

  this->timebars->lightBars.updateShapes();
  this->timebars->lighterBars.updateShapes();
  */
}


}
