#include "SlotHandle.hpp"
#include "SlotView.hpp"
#include <QCursor>
#include <QApplication>
#include <QPainter>
#include <QPalette>

SlotHandle::SlotHandle(const SlotView &slotView, QGraphicsItem *parent):
    QGraphicsItem{parent},
    m_slotView{slotView},
    m_width{slotView.boundingRect().width()}
{
    this->setCursor(Qt::SizeVerCursor);

    QPalette palette = qApp->palette("ScenarioPalette").base().color();
    auto col = palette.background().color();
    col.setAlphaF(0.2);

    m_pen.setColor(col);
    m_pen.setCosmetic(true);
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
    painter->setPen(m_pen);
    painter->drawLine(QPointF{0., 0.}, QPointF{m_width, 0.});
}

void SlotHandle::setWidth(qreal width)
{
    m_width = width;
    prepareGeometryChange();
}
