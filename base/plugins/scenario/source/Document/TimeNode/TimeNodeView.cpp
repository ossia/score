#include "TimeNodeView.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPointF>

#include <QDebug>

TimeNodeView::TimeNodeView(QGraphicsObject *parent):
    QGraphicsObject{parent}
{
    this->setParentItem(parent);
    this->setZValue(parent->zValue() + 1);
    this->setFlag(ItemIsSelectable);
    this->setAcceptHoverEvents(true);
}

void TimeNodeView::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget)
{
    painter->setPen(QPen(QBrush(QColor(180,0,220)), 5, Qt::SolidLine));
    painter->drawLine(0, m_top, 0, m_bottom);

    painter->setPen(QPen(QBrush(QColor(0,200,0)), 1, Qt::SolidLine));
    painter->drawRect(boundingRect());
}

QRectF TimeNodeView::boundingRect() const
{
    return {-10, (qreal) m_top, 20, (qreal) (m_bottom-m_top) };
}

void TimeNodeView::setExtremities(int top, int bottom)
{
    m_top = top - 10;
    m_bottom = bottom + 10;
    this->update();
}

void TimeNodeView::mousePressEvent(QGraphicsSceneMouseEvent *m)
{
    QGraphicsObject::mousePressEvent(m);

    m_clickedPoint =  m->pos();
}

void TimeNodeView::mouseReleaseEvent(QGraphicsSceneMouseEvent *m)
{
    emit timeNodeReleased( pos() + m->pos() - m_clickedPoint );
}
