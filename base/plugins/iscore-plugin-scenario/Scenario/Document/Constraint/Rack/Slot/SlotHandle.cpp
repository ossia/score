#include <Process/Style/ScenarioStyle.hpp>
#include <qnamespace.h>
#include <QPainter>
#include <QPoint>
#include <QCursor>
#include <QGraphicsSceneEvent>

#include "SlotHandle.hpp"
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
SlotHandle::SlotHandle(const SlotView &slotView, QGraphicsItem *parent):
    QGraphicsItem{parent},
    m_slotView{slotView},
    m_width{slotView.boundingRect().width()}
{
    this->setCursor(Qt::SizeVerCursor);
    m_pen.setWidth(0);
}

QRectF SlotHandle::boundingRect() const
{
    return {0, -handleHeight() / 2., m_width, handleHeight()};
}

void SlotHandle::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    m_pen.setColor(ScenarioStyle::instance().ProcessViewBorder.getColor());
    painter->setPen(m_pen);
    painter->setBrush(m_pen.color());

    painter->drawLine(0, -handleHeight() / 2., m_width, -handleHeight() / 2.);
}

void SlotHandle::setWidth(qreal width)
{
    m_width = width;
    prepareGeometryChange();
}

void SlotHandle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_slotView.presenter.pressed(event->scenePos());
}

void SlotHandle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    m_slotView.presenter.moved(event->scenePos());
}

void SlotHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    m_slotView.presenter.released(event->scenePos());
}
}
