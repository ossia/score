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
    m_trigger = new ConditionView(this);
    m_trigger->setVisible(false);
    m_trigger->setPos(-7, -7);
    this->setParentItem(parent);
    this->setCursor(Qt::CrossCursor);


    this->setZValue(parent->zValue() + 2);
    this->setAcceptHoverEvents(true);

    m_color = Qt::white;
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
    QColor highlight = QColor::fromRgbF(0.188235, 0.54902, 0.776471);

    // Rect
    if(isSelected())
    {
        pen_color = highlight;
    }
    else if (isShadow())
    {
        pen_color =highlight.lighter();
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


QRectF ConditionView::boundingRect() const
{
    return  QRectF{QPointF{0, 0}, QSizeF{25,14}};
}


void ConditionView::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    QPen pen(Qt::white);
    pen.setWidth(3);
    painter->setPen(pen);
    painter->setBrush(Qt::white);

    int csize = 50;
    QRectF square(boundingRect().topLeft(), QSize(boundingRect().height(),
                                                  boundingRect().height()));
    painter->drawArc(square, (360 + csize) * 16, (360 - 2 * csize) * 16);

    static const QPointF triangle[3] = {
        QPointF(15, 3),
        QPointF(15, 10),
        QPointF(20, 7)
    };
    pen.setWidth(1);
    painter->setPen(pen);
    painter->drawPolygon(triangle, 3);
}
