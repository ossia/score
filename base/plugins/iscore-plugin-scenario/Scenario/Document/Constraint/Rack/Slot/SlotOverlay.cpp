#include <Process/Style/ScenarioStyle.hpp>
#include <QGraphicsSceneEvent>
#include <QPainter>

#include "SlotOverlay.hpp"
#include "SlotPresenter.hpp"
#include "SlotView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
SlotOverlay::SlotOverlay(SlotView *parent):
    QGraphicsItem{parent},
    m_slotView{*parent}
{
    this->setZValue(1500);
    this->setPos(0, 0);
}

QRectF SlotOverlay::boundingRect() const
{
    const auto& rect = m_slotView.boundingRect();
    return {0, 0, rect.width(), rect.height() - 5};
}

void SlotOverlay::setHeight(qreal height)
{
    prepareGeometryChange();
}
void SlotOverlay::setWidth(qreal width)
{
    prepareGeometryChange();
}


void SlotOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(ScenarioStyle::instance().SlotOverlayBorder);
    painter->setBrush(ScenarioStyle::instance().SlotOverlay);
    painter->drawRect(boundingRect());
}

void SlotOverlay::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_slotView.presenter.pressed(event->scenePos());
}

void SlotOverlay::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_slotView.presenter.moved(event->scenePos());
}

void SlotOverlay::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_slotView.presenter.released(event->scenePos());
}
}
