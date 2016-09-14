#pragma once
#include "CurvePoint.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <QState>
class QGraphicsItem;

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
