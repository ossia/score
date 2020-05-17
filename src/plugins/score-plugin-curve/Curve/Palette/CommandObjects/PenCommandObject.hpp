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
  using seg_tuple
      = std::tuple<optional<SegmentData>, optional<SegmentData>, std::vector<SegmentData>>;
  void release_n(seg_tuple&&);
  seg_tuple filterSegments();
  PointArraySegment m_segment;

  Curve::StateBase* m_state{};
  QPointF m_originalPress;
  QPointF m_minPress{}, m_maxPress{};
};
}
