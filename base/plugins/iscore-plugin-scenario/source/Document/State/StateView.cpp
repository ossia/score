#include "StateView.hpp"
#include "StatePresenter.hpp"
#include <iscore/tools/Todo.hpp>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QDebug>
#include <ProcessInterface/Style/ScenarioStyle.hpp>

StateView::StateView(StatePresenter& pres, QGraphicsItem* parent) :
    QGraphicsObject(parent),
    m_presenter{pres}
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 5);
    this->setAcceptDrops(true);
    this->setAcceptHoverEvents(true);
}

const StatePresenter &StateView::presenter() const
{
    return m_presenter;
}



void StateView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen statePen = ScenarioStyle::instance().stateOutline;
    statePen.setWidth(2);
    QBrush stateBrush = m_baseColor;
    QColor highlight = ScenarioStyle::instance().stateSelected;

    painter->setPen(statePen);
    painter->setBrush(stateBrush);
    if(m_selected)
        painter->setBrush(highlight);

    qreal radius = m_containMessage ? m_radiusFull : m_radiusVoid;
    painter->drawEllipse({0., 0.}, radius, radius);
#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter->setBrush(Qt::NoBrush);
    painter->setPen(Qt::darkYellow);
    painter->drawRect(boundingRect());
#endif
}

void StateView::setContainMessage(bool arg)
{
    m_containMessage = arg;
    update();
}

void StateView::setSelected(bool arg)
{
    m_selected = arg;
    update();
}

void StateView::changeColor(const QColor & c)
{
    m_baseColor = c;
    update();
}

void StateView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
        emit m_presenter.pressed(event->scenePos());
}

void StateView::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    emit m_presenter.moved(event->scenePos());
}

void StateView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit m_presenter.released(event->scenePos());
}

void StateView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    emit dropReceived(event->mimeData());
}

