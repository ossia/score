#pragma once
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/tools/std/Optional.hpp>

#include <QPoint>

namespace Curve
{
class Model;
class Presenter;
class StateBase;
class SCORE_PLUGIN_CURVE_EXPORT PenCommandObject final : public CommandObjectBase
{
public:
  PenCommandObject(Presenter* presenter, const score::CommandStackFacade&);

  void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }

  void on_press() override;

  void move();

  void release();

  void cancel();

private:
  struct FilteredSegments
  {
    std::optional<Id<Curve::SegmentModel>> id_before_middleBegin;
    std::optional<Id<Curve::SegmentModel>> id_after_middleEnd;
    std::optional<SegmentData> middleBegin;
    std::optional<SegmentData> middleEnd;
    std::vector<SegmentData> segments;
  };

  void release_n(FilteredSegments&&);
  FilteredSegments filterSegments();
  PointArraySegment m_segment;

  Curve::StateBase* m_state{};
  QPointF m_originalPress;
  QPointF m_minPress{}, m_maxPress{};
};
}
