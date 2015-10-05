#include "EventView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include "EventPresenter.hpp"
#include "EventModel.hpp"
#include "ConditionView.hpp"
#include <ProcessInterface/Style/ScenarioStyle.hpp>

EventView::EventView(EventPresenter& presenter,
                     QGraphicsObject* parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    setAcceptDrops(true);

    m_conditionItem = new ConditionView(this);
    m_conditionItem->setVisible(false);
    m_conditionItem->setPos(-13.5, -13.5);

    this->setParentItem(parent);
    this->setCursor(Qt::SizeHorCursor);

    this->setZValue(parent->zValue() + 2);
    this->setAcceptHoverEvents(true);

    m_color = presenter.model().metadata.color();
}

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
//    m_triggerItem->setVisible(!trig.isEmpty());
//    m_triggerItem->setToolTip(m_condition);
}

bool EventView::hasTrigger() const
{
    return !m_trigger.isEmpty();
}




void EventView::paint(QPainter* painter,
                      const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    QPen eventPen;
    if(m_status == EventStatus::Editing)
        eventPen = QPen(m_color);
    else
        eventPen = QPen(eventStatusColor(m_status));

    if(isSelected())
    {
        eventPen = QPen(ScenarioStyle::instance().eventSelected);
    }
    eventPen.setWidth(2);


    QPen pen{QBrush(eventPen.color()), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
    painter->setPen(pen);

    painter->drawRect(QRectF(QPointF(0, 0), QPointF(0, m_extent.bottom() - m_extent.top())));

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter->setPen(Qt::cyan);
    painter->drawRect(boundingRect());
#endif
}


void EventView::setExtent(const VerticalExtent& extent)
{
    prepareGeometryChange();
    m_extent = extent;
    this->update();
}

void EventView::setExtent(VerticalExtent &&extent)
{
    prepareGeometryChange();
    m_extent = std::move(extent);
    this->update();
}

void EventView::setStatus(EventStatus s)
{
    m_status = s;
    update();
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

void EventView::changeColor(QColor newColor)
{
    m_color = newColor;
    this->update();
}

void EventView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
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
    emit dropReceived(event->scenePos(), event->mimeData());
}
