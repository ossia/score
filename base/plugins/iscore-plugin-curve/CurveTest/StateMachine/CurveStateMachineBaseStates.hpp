#pragma once
#include "CurvePoint.hpp"
#include <QState>
class QGraphicsItem;

namespace Curve
{
class StateBase : public QState
{
    public:
        const QGraphicsItem*  clickedItem{}, *hoveredItem{};
        CurvePoint currentPoint;
};
}
