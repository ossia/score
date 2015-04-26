#include "EventView.hpp"

#include <QPainter>
#include <QGraphicsScene>
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
    // Ball
    painter->setBrush(pen_color);
    painter->setPen(pen_color);
    painter->drawEllipse(boundingRect().center(), 5, 5);
}

void EventView::changeColor(QColor newColor)
{
    m_color = newColor;
    this->update();
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
