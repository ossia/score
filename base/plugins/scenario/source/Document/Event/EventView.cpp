#include "EventView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>

EventView::EventView(const EventPresenter& presenter,
                     QGraphicsObject* parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 2);
    this->setAcceptHoverEvents(true);

    m_color = Qt::black;
}

QRectF EventView::boundingRect() const
{
    return { -5, -5, 10, 10};
}

void EventView::paint(QPainter* painter,
                      const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    QColor pen_color = m_color;

    // Rect
    if(isSelected())
    {
        pen_color = Qt::blue;
    }
    else if (isShadow())
    {
        pen_color = Qt::cyan;
    }

    /*	else if(parentItem()->isSelected())
        {
            pen_color = Qt::cyan;
        }
    */
    //painter->drawRect(boundingRect());

    (m_moving ? pen_color.setAlphaF(0.4) : pen_color.setAlphaF(1.0));

    // Ball
    painter->setBrush(pen_color);
    painter->setPen(pen_color);
    painter->drawEllipse(boundingRect().center(), 5, 5);

//	painter->setPen(QPen(QBrush(QColor(0,0,0)), 1, Qt::SolidLine));

}

void EventView::changeColor(QColor newColor)
{
    m_color = newColor;
    this->update();
}

void EventView::setMoving(bool arg)
{
    m_moving = arg;
    update();
}

void EventView::setShadow(bool arg)
{
    m_shadow = arg;
    update();
}

void EventView::hoverEnterEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsObject::hoverEnterEvent(h);
    emit eventHoverEnter();
}

void EventView::hoverLeaveEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsObject::hoverLeaveEvent(h);
    emit eventHoverLeave();
}
