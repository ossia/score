#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
namespace Scenario
{
using TimeSignatureMap = ossia::flat_map<TimeVal, Control::time_signature>;
struct Timebars;
class LightBars;
class LighterBars;
struct MusicalGrid
{
  const TimeSignatureMap& measures;
  Timebars* timebars{};
  TimeVal& magneticDivision;

  using timings = std::pair<double, ossia::bar_time>;
  std::vector<timings> mainPositions;
  std::vector<timings> subPositions;
  int32_t startBar = 0;
  void compute(
        TimeVal timeDelta
      , ZoomRatio m_zoomRatio
      , QRectF sceneRect
      , TimeVal x0_time);
};

}
