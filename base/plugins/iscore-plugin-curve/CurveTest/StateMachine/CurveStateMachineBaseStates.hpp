#pragma once
#include "CurvePoint.hpp"
#include <QState>
class QGraphicsItem;

namespace Curve
{
class StateBase : public QState
{
    public:
        // TODO can't do this... Instead save ids (and for the point save {idbefore, idafter}
        const QGraphicsItem*  clickedItem{}, *hoveredItem{};
        CurvePoint currentPoint;
};
}
