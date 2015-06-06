#pragma once
#include "CurvePoint.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <QState>
class CurveSegmentModel;
struct CurvePointId
{
        id_type<CurveSegmentModel> previous;
        id_type<CurveSegmentModel> following;
};

class QGraphicsItem;

namespace Curve
{
class StateBase : public QState
{
    public:
        id_type<CurveSegmentModel> clickedSegmentId;
        CurvePointId clickedPointId;

        id_type<CurveSegmentModel> hoveredSegmentId;
        CurvePointId hoveredPointId;

        CurvePoint currentPoint;
};
}
