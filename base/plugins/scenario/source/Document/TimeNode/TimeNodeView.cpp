#include "TimeNodeView.hpp"
#include <QGraphicsScene>
#include <QPainter>
#include <QPointF>

TimeNodeView::TimeNodeView(QGraphicsObject *parent):
    QGraphicsObject{parent}
{
    this->setParentItem(parent);
}

void TimeNodeView::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget)
{
    painter->setPen(Qt::black);
    painter->drawLine(0, m_top, 0, m_bottom);
}

