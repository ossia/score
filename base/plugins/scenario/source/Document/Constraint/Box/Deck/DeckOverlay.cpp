#include "DeckOverlay.hpp"
#include "DeckPresenter.hpp"
#include "DeckView.hpp"
#include "DeckHandle.hpp"

#include <QCursor>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

DeckOverlay::DeckOverlay(DeckView *parent):
    QGraphicsItem{parent},
    deckView{*parent},
    m_handle{new DeckHandle{deckView, this}}
{
    this->setZValue(1500);
    this->setPos(0, 0);
    m_handle->setPos(0, this->boundingRect().height() - DeckHandle::handleHeight());
}

QRectF DeckOverlay::boundingRect() const
{
    const auto& rect = deckView.boundingRect();
    return {0, 0, rect.width(), rect.height()};
}

void DeckOverlay::setHeight(qreal height)
{
    prepareGeometryChange();
    m_handle->setPos(0, this->boundingRect().height() - DeckHandle::handleHeight());
}
void DeckOverlay::setWidth(qreal width)
{
    prepareGeometryChange();
    m_handle->setWidth(width);
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
    emit deckView.presenter.pressed(event->scenePos());
}

void DeckOverlay::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit deckView.presenter.moved(event->scenePos());
}

void DeckOverlay::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit deckView.presenter.released(event->scenePos());
}
