#pragma once
#include "CurvePoint.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <QState>
class CurveSegmentModel;
struct CurvePointId
{
        Id<CurveSegmentModel> previous;
        Id<CurveSegmentModel> following;
};

class QGraphicsItem;

namespace Curve
{
class StateBase : public QState
{
    public:
        using QState::QState;
        Id<CurveSegmentModel> clickedSegmentId;
        CurvePointId clickedPointId;

        Id<CurveSegmentModel> hoveredSegmentId;
        CurvePointId hoveredPointId;

        CurvePoint currentPoint;
};
}
