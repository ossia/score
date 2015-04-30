#include "EventView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include "EventPresenter.hpp"
#include <QApplication>


EventView::EventView(EventPresenter& presenter,
                     QGraphicsObject* parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    m_trigger = new TriggerView(this);
    m_trigger->setVisible(false);
    m_trigger->setPos(0, -5);
    this->setParentItem(parent);
    this->setCursor(Qt::CrossCursor);


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

void EventView::setCondition(const QString &cond)
{
    m_condition = cond;
    m_trigger->setVisible(!cond.isEmpty());
    m_trigger->setToolTip(m_condition);
}

QRectF EventView::boundingRect() const
{
    return {-3, -3, 6, 6};
}

void EventView::paint(QPainter* painter,
                      const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    QColor pen_color = m_color;

    // Rect
    if(isSelected())
    {
        pen_color = QApplication::palette().highlight().color();
    }
    else if (isShadow())
    {
        pen_color = QApplication::palette().highlight().color().lighter();
    }

    // Ball
    painter->setBrush(pen_color);
    painter->setPen(pen_color);
    painter->drawEllipse({0., 0.}, 3., 3.);
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

bool EventView::hasCondition() const
{
    return !m_condition.isEmpty();
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


QRectF TriggerView::boundingRect() const
{
    return  QRectF{QPointF{-5, -10}, QSizeF{10, 10}};
}


void TriggerView::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *option,
                        QWidget *widget)
{
    painter->setPen(Qt::black);
    painter->setBrush(Qt::darkGray);
    static const QPointF triangle[3] = {
        QPointF(0, 0),
        QPointF(-5, -10),
        QPointF(5, -10),
    };
    painter->drawPolygon(triangle, 3);
}
