#include "CurveView.hpp"
#include <QPainter>
QRectF CurveView::boundingRect() const
{
    return rect;
}

void CurveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
}

#include <QDebug>
void CurveView::updateSubitems()
{

}


void CurveView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    rect = theRect;
    updateSubitems();
}
