#include <Process/Style/ScenarioStyle.hpp>
#include <qnamespace.h>
#include <QPainter>
#include <QPoint>
#include <QCursor>

#include "SlotHandle.hpp"
#include "SlotView.hpp"

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
    return {0, 0, m_width, handleHeight()};
}

void SlotHandle::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    m_pen.setColor(ScenarioStyle::instance().ProcessViewBorder.getColor());
    painter->setPen(m_pen);
    painter->setBrush(m_pen.color());
    painter->drawRect(boundingRect());
}

void SlotHandle::setWidth(qreal width)
{
    m_width = width;
    prepareGeometryChange();
}
}
