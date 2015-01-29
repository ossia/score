#include "TimeNodeView.hpp"

#include <QGraphicsScene>
#include <QPainter>
#include <QPointF>

TimeNodeView::TimeNodeView(QGraphicsObject *parent):
    QGraphicsObject{parent}
{
    this->setParentItem(parent);
    this->setZValue(parent->zValue() + 1);
}

void TimeNodeView::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget)
{
    painter->setPen(QPen(QBrush(QColor(180,0,220)), 2, Qt::SolidLine));
    painter->drawLine(0, m_top, 0, m_bottom);
}

QRectF TimeNodeView::boundingRect() const
{
    return {0, 0, 5,(qreal) (m_bottom-m_top) };
}

void TimeNodeView::setExtremities(int top, int bottom)
{
    m_top = top - 10;
    m_bottom = bottom + 10;
    this->update();
}

