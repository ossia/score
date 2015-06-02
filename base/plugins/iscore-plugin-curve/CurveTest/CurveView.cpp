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

    if(m_selectArea != QRectF{})
    {
        painter->setCompositionMode(QPainter::CompositionMode_Xor);
        painter->setPen(QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});

        painter->drawRect(m_selectArea);
    }
}

void CurveView::setSelectionArea(const QRectF& rect)
{
    m_selectArea = rect;
    update();
}

void CurveView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    m_rect = theRect;
}
