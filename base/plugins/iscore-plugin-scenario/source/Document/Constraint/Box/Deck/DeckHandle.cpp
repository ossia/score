#include "DeckHandle.hpp"
#include "DeckView.hpp"
#include <QCursor>
#include <QApplication>
#include <QPainter>
#include <QPalette>

DeckHandle::DeckHandle(const DeckView &deckView, QGraphicsItem *parent):
    QGraphicsItem{parent},
    m_deckView{deckView},
    m_width{deckView.boundingRect().width()}
{
    this->setCursor(Qt::SizeVerCursor);

    QPalette palette = qApp->palette("ScenarioPalette").base().color();
    auto col = palette.background().color();
    col.setAlphaF(0.2);

    m_pen.setColor(col);
    m_pen.setCosmetic(true);
    m_pen.setWidth(0);
}

QRectF DeckHandle::boundingRect() const
{
    return {0, 0, m_width, handleHeight()};
}

void DeckHandle::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    painter->setPen(m_pen);
    painter->drawLine(QPointF{0., 0.}, QPointF{m_width, 0.});
}

void DeckHandle::setWidth(qreal width)
{
    m_width = width;
    prepareGeometryChange();
}
