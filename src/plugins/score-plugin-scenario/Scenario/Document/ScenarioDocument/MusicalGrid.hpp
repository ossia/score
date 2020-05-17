#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
namespace Scenario
{
using TimeSignatureMap = ossia::flat_map<TimeVal, Control::time_signature>;
struct Timebars;
class LightBars;
class LighterBars;

class MusicalGrid : public QObject
{
  W_OBJECT(MusicalGrid)
public:
  MusicalGrid(Timebars& timebars)
    : timebars{timebars}
  {
  }

  Timebars& timebars;
  void setMeasures(const TimeSignatureMap& m)
  {
    m_measures = &m;
  }

  struct timings {
    double pos_x{};
    ossia::bar_time timings;
    ossia::bar_time increment;
  };

  std::vector<timings> mainPositions;
  std::vector<timings> subPositions;

  void changed() W_SIGNAL(changed);

  void compute(
        TimeVal timeDelta
      , ZoomRatio m_zoomRatio
      , QRectF sceneRect
      , TimeVal x0_time);

  const TimeSignatureMap* m_measures{};
};

}
