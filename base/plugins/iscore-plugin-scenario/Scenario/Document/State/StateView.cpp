#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QGraphicsSceneEvent>
#include <qnamespace.h>
#include <QPainter>
#include <QPen>

#include "StatePresenter.hpp"
#include "StateView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;
namespace Scenario
{
StateView::StateView(StatePresenter& pres, QGraphicsItem* parent) :
    QGraphicsObject(parent),
    m_presenter{pres}
{
    this->setParentItem(parent);

    this->setZValue(ZPos::State);
    this->setAcceptDrops(true);
    this->setAcceptHoverEvents(true);
    m_baseColor = ScenarioStyle::instance().StateOutline;
}

void StateView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QBrush temporalPointBrush = m_selected ? ScenarioStyle::instance().StateSelected : ScenarioStyle::instance().StateDot;
    QBrush stateBrush = m_baseColor;

    auto status = m_status.get();
    if(status != ExecutionStatus::Editing)
        temporalPointBrush = m_status.stateStatusColor();

    if(m_containMessage)
    {
        painter->setBrush(stateBrush);
        painter->drawEllipse({0., 0.}, m_radiusFull, m_radiusFull);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(temporalPointBrush);
    qreal r = m_radiusPoint;
    painter->drawEllipse({0., 0.}, r, r);




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
    setDilatation(m_selected ? 1.5 : 1);
}

void StateView::changeColor(const QColor & c)
{
    m_baseColor = c;
    update();
}

void StateView::setStatus(ExecutionStatus status)
{
    if(m_status.get() == status)
        return;
    m_status.set(status);
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

void StateView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
//   m_dilatationFactor = 1.5;
   update();
}

void StateView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
//    m_dilatationFactor = m_selected ? 1.5 : 1;
    update();
}
void StateView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    setDilatation(1.5);
}

void StateView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
    setDilatation(m_selected ? 1.5 : 1);
}


void StateView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    emit dropReceived(event->mimeData());
}

void StateView::setDilatation(double val)
{
    m_dilatationFactor = val;
    this->setScale(m_dilatationFactor);
    update();
}
}
