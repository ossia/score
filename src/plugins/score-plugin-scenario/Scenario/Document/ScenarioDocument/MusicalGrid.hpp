#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
namespace Scenario
{
using TimeSignatureMap = ossia::flat_map<TimeVal, Control::time_signature>;
struct Timebars;
class LightBars;
class LighterBars;

struct MusicalGrid : public QObject
{
  W_OBJECT(MusicalGrid)
public:
  MusicalGrid(const TimeSignatureMap& measures, Timebars& timebars)
    : measures{measures}
    , timebars{timebars}
  {
  }

  const TimeSignatureMap& measures;
  Timebars& timebars;

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
};

}
