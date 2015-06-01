#include "CurveView.hpp"
#include <QPainter>

QRectF CurveView::boundingRect() const
{
    return m_rect;
}

void CurveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
}

void CurveView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    m_rect = theRect;
}
