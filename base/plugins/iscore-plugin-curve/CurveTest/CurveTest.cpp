#include "CurveTest.hpp"
#include <QPainter>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

inline double clamp(double val, double min, double max)
{
    return val < min ? min : (val > max ? max : val);
}


QPointF myscale(QPointF first, QSizeF second)
{
    return {first.x() * second.width(), first.y() * second.height()};
}



