#include "EventView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include "EventPresenter.hpp"

EventView::EventView(EventPresenter& presenter,
                     QGraphicsObject* parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 2);
    this->setAcceptHoverEvents(true);

    m_color = Qt::black;
}

int EventView::type() const
{ return QGraphicsItem::UserType + 1; }

const EventPresenter& EventView::presenter() const
{
    return m_presenter;
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

void EventView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

bool EventView::isSelected() const
{
    return m_selected;
}

bool EventView::isShadow() const
{
    return m_shadow;
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

void EventView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.pressed(event->scenePos());
}

void EventView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.moved(event->scenePos());
}

void EventView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.released(event->scenePos());
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
