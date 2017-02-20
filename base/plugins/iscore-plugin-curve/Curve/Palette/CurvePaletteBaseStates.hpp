#pragma once
#include "CurvePoint.hpp"
#include <QState>
#include <iscore/model/Identifier.hpp>
class QQuickPaintedItem;

namespace Curve
{
class SegmentModel;

struct PointId
{
  OptionalId<SegmentModel> previous;
  OptionalId<SegmentModel> following;
};

class StateBase : public QState
{
public:
  using QState::QState;
  Id<SegmentModel> clickedSegmentId;
  PointId clickedPointId;

  Id<SegmentModel> hoveredSegmentId;
  PointId hoveredPointId;

  Curve::Point currentPoint;
};
}
