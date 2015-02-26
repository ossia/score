#include "TimeNodeView.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPointF>

#include <QDebug>

TimeNodeView::TimeNodeView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{
    this->setParentItem(parent);
    this->setZValue(parent->zValue() + 1.5);
    this->setFlag(ItemIsSelectable);
    this->setAcceptHoverEvents(true);

    m_color = Qt::darkRed;
}

void TimeNodeView::paint(QPainter* painter,
                         const QStyleOptionGraphicsItem* option,
                         QWidget* widget)
{
    QColor pen_color = m_color;

    if(isSelected())
    {
        pen_color = Qt::blue;
    }

    /*    else if(parentItem()->isSelected())
        {
            pen_color = Qt::cyan;
        }
    */
    (m_moving ? pen_color.setAlphaF(0.4) : pen_color.setAlphaF(1.0));
    painter->setBrush(pen_color);
    painter->setPen(QPen(QBrush(pen_color), 5, Qt::SolidLine));

    painter->drawLine(0, m_top, 0, m_bottom);

//    painter->setPen(QPen(QBrush(QColor(0,200,0)), 1, Qt::SolidLine));
//    painter->drawRect(boundingRect());

    m_moving = false;
}

QRectF TimeNodeView::boundingRect() const
{
    return { -5, (qreal) m_top, 10, (qreal)(m_bottom - m_top) };
}

void TimeNodeView::setExtremities(int top, int bottom)
{
    m_top = top - 10;
    m_bottom = bottom + 10;
    this->update();
}

void TimeNodeView::mousePressEvent(QGraphicsSceneMouseEvent* m)
{
    QGraphicsObject::mousePressEvent(m);

    m_clickedPoint =  m->pos();

    emit timeNodeSelected();
}

void TimeNodeView::mouseMoveEvent(QGraphicsSceneMouseEvent* m)
{
    QGraphicsObject::mouseMoveEvent(m);
    m_moving = true;

    if(m->pos() != m_clickedPoint)
    {
        emit timeNodeMoved(pos() + m->pos() - m_clickedPoint);
    }
}

void TimeNodeView::mouseReleaseEvent(QGraphicsSceneMouseEvent* m)
{
    QGraphicsObject::mouseReleaseEvent(m);
    m_moving = false;
    emit timeNodeReleased();
}

void TimeNodeView::setMoving(bool arg)
{
    m_moving = arg;
}

void TimeNodeView::changeColor(QColor newColor)
{
    m_color = newColor;
    this->update();
}
