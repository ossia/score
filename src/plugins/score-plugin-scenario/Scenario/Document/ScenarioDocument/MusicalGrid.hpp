#pragma once
#include <Process/TimeValue.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/TimeSignatureMap.hpp>

#include <ossia-qt/time.hpp>

namespace Scenario
{
struct Timebars;
class LightBars;
class LighterBars;

class SCORE_PLUGIN_SCENARIO_EXPORT MusicalGrid : public QObject
{
  W_OBJECT(MusicalGrid)
public:
  MusicalGrid(Timebars& timebars)
      : timebars{timebars}
  {
  }

  Timebars& timebars;
  void setMeasures(const TimeSignatureMap& m) { m_measures = &m; }

  struct timings
  {
    double pos_x{};
    ossia::bar_time timings;
    ossia::bar_time increment;
  };

  std::vector<timings> mainPositions;
  //std::vector<timings> subPositions;

  void changed() W_SIGNAL(changed);

  void
  compute(TimeVal timeDelta, ZoomRatio m_zoomRatio, QRectF sceneRect, TimeVal x0_time);

  const TimeSignatureMap* m_measures{};

  //! For each time signature of m_measures, the bars counted before it.
  std::vector<int32_t> m_barsAtSignature;

private:
  // What the lines were last computed for.
  TimeSignatureMap m_computedMeasures;
  TimeVal m_computedTimeDelta{};
  ZoomRatio m_computedZoom{};
  QRectF m_computedRect{};
  TimeVal m_computedX0{};
  bool m_computed{};
};

}
