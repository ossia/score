#include "ConditionView.hpp"
#include <QPainter>
QRectF ConditionView::boundingRect() const
{
    return  QRectF{QPointF{0, 0}, QSizeF{25,14}};
}


void ConditionView::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    QPen pen(Qt::white);
    pen.setWidth(3);
    painter->setPen(pen);
    painter->setBrush(Qt::white);

    int csize = 50;
    QRectF square(boundingRect().topLeft(), QSize(boundingRect().height(),
                                                  boundingRect().height()));
    painter->drawArc(square, (360 + csize) * 16, (360 - 2 * csize) * 16);

    static const QPointF triangle[3] = {
        QPointF(15, 3),
        QPointF(15, 10),
        QPointF(20, 7)
    };
    pen.setWidth(1);
    painter->setPen(pen);
    painter->drawPolygon(triangle, 3);
}
