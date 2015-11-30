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

StateView::StateView(StatePresenter& pres, QGraphicsItem* parent) :
    QGraphicsObject(parent),
    m_presenter{pres}
{
    this->setParentItem(parent);

    this->setZValue(5);
    this->setAcceptDrops(true);
    this->setAcceptHoverEvents(true);
}

void StateView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen statePen = ScenarioStyle::instance().StateOutline;
    statePen.setWidth(2);
    QBrush stateBrush = m_baseColor;
    QPen highlightPen = ScenarioStyle::instance().StateSelected;
    highlightPen.setWidth(4);

    painter->setPen(statePen);

    if(m_selected)
        painter->setPen(highlightPen);

    // qreal radius = m_containMessage ? m_radiusFull : m_radiusVoid;

    if(m_containMessage)
        painter->setBrush(stateBrush);

    painter->drawEllipse({0., 0.}, m_radiusFull, m_radiusFull);



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

