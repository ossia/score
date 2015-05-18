#include "DeckHandle.hpp"
#include "DeckView.hpp"
#include <QCursor>
#include <QApplication>
#include <QPainter>
#include <QPalette>

DeckHandle::DeckHandle(const DeckView &deckView, QGraphicsItem *parent):
    QGraphicsItem{parent},
    deckView{deckView},
    m_width{deckView.boundingRect().width()}
{
    this->setCursor(Qt::SizeVerCursor);
}

QRectF DeckHandle::boundingRect() const
{
    return {0, 0, m_width, handleHeight()};
}

void DeckHandle::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPalette palette{QApplication::palette()};
    painter->setBrush(palette.midlight());
    painter->drawRect(boundingRect());
}

void DeckHandle::setWidth(qreal width)
{
    m_width = width;
    prepareGeometryChange();
}
