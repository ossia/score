#include "DeckOverlay.hpp"
#include "DeckPresenter.hpp"
#include "DeckView.hpp"
#include "DeckHandle.hpp"

#include <QCursor>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>
#include <QPalette>

DeckOverlay::DeckOverlay(DeckView *parent):
    QGraphicsItem{parent},
    m_deckView{*parent}
{
    this->setZValue(1500);
    this->setPos(0, 0);
}

QRectF DeckOverlay::boundingRect() const
{
    const auto& rect = m_deckView.boundingRect();
    return {0, 0, rect.width(), rect.height() - 5};
}

void DeckOverlay::setHeight(qreal height)
{
    prepareGeometryChange();
}
void DeckOverlay::setWidth(qreal width)
{
    prepareGeometryChange();
}


void DeckOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPalette palette{QApplication::palette()};

    painter->setPen(Qt::black);
    painter->setBrush(QColor(170, 170, 170, 70));
    painter->drawRect(boundingRect());
}

void DeckOverlay::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_deckView.presenter.pressed(event->scenePos());
}

void DeckOverlay::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_deckView.presenter.moved(event->scenePos());
}

void DeckOverlay::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_deckView.presenter.released(event->scenePos());
}
