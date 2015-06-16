#include "EventView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include "EventPresenter.hpp"
#include "ConditionView.hpp"
#include "TriggerView.hpp"
#include <QApplication>
#include <QPalette>

static const qreal radius = 10.;
static const qreal radiusIn = 3.;

EventView::EventView(EventPresenter& presenter,
                     QGraphicsObject* parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    setAcceptDrops(true);

    m_conditionItem = new ConditionView(this);
    m_conditionItem->setVisible(false);
    m_conditionItem->setPos(-13.5, -13.5);

    m_triggerItem = new TriggerView(this);
    m_triggerItem->setVisible(false);

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
    if(m_condition == cond)
        return;
    m_condition = cond;
    m_conditionItem->setVisible(!cond.isEmpty());
    m_conditionItem->setToolTip(m_condition);
}

bool EventView::hasCondition() const
{
    return !m_condition.isEmpty();
}

void EventView::setTrigger(const QString &trig)
{
    if(m_trigger == trig)
        return;
    m_trigger = trig;
    m_triggerItem->setVisible(!trig.isEmpty());
    m_triggerItem->setToolTip(m_condition);
}

bool EventView::hasTrigger() const
{
    return !m_trigger.isEmpty();
}

void EventView::setHalves(Scenario::EventHalves h)
{
    m_halves = h;
    update();
}

QRectF EventView::boundingRect() const
{
    return {- radius, -radius, 2 * radius, 2 * radius};
}

void EventView::paint(QPainter* painter,
                      const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    QPen eventPen = m_color;
    QColor highlight = QColor::fromRgbF(0.188235, 0.54902, 0.776471);

    // Rect
    if(isSelected())
    {
        eventPen = QPen(highlight);
    }
    else if (isShadow())
    {
        eventPen = QPen(highlight.lighter());
    }

    eventPen.setWidth(2);
    // Ball
/*
    painter->setPen(eventPen);
    painter->setBrush(Qt::white);

    painter->drawEllipse({0., 0.}, radiusIn, radiusIn);
*/
    painter->setPen(Qt::darkCyan);
//    painter->drawRect(boundingRect());
    painter->setBrush(Qt::white);
    eventPen.setWidth(1);
    painter->setPen(eventPen);
    if((int)m_halves & (int)Scenario::EventHalves::Before)
    {
        painter->drawChord(this->boundingRect(), 90 * 16, 180 * 16);
		painter->drawChord(this->boundingRect(), 270 * 16, 180 * 16);
    }

    if((int)m_halves & (int)Scenario::EventHalves::After)
    {
		painter->drawChord(this->boundingRect(), 90 * 16, 180 * 16);
        painter->drawChord(this->boundingRect(), 270 * 16, 180 * 16);
    }
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

#include <QMimeData>
void EventView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    emit dropReceived(event->mimeData());
}
