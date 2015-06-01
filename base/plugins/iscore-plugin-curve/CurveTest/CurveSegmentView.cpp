#include "CurveSegmentView.hpp"
#include <QPainter>

void CurveSegmentView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    rect = theRect;
}

void CurveSegmentView::setPoints(QVector<QPointF>&& thePoints)
{
    points = std::move(thePoints);
    update();
}

QRectF CurveSegmentView::boundingRect() const
{
    return rect;
}

void CurveSegmentView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::red);
    for(int i = 0; i < points.size() - 1; i++)
        painter->drawLine(points[i], points[i+1]);

    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
}
