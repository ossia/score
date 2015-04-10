#include "DeckView.hpp"

#include <iscore/tools/NamedObject.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>


DeckView::DeckView(const DeckPresenter &pres, QGraphicsObject* parent) :
    QGraphicsObject {parent},
    presenter{pres}
{
    this->setZValue(parent->zValue() + 1);
}

QRectF DeckView::boundingRect() const
{
    return {0, 0, m_width, m_height};
}

void DeckView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    auto rect = boundingRect();
    painter->drawRect(rect);
    painter->setBrush(QBrush {Qt::darkGray});

    painter->drawRect(0, rect.height() - handleHeight(), rect.width(), handleHeight());
}

void DeckView::setHeight(qreal height)
{
    prepareGeometryChange();
    m_height = height;
}

qreal DeckView::height() const
{
    return m_height;
}

void DeckView::setWidth(qreal width)
{
    prepareGeometryChange();
    m_width = width;
}

qreal DeckView::width() const
{
    return m_width;
}

void DeckView::enable()
{
    for(QGraphicsItem* item : childItems())
    {
        item->setEnabled(true);
        item->setVisible(true);
        item->setFlag(ItemStacksBehindParent, false);
    }
    delete m_overlay;
}

void DeckView::disable()
{
    for(QGraphicsItem* item : childItems())
    {
        item->setEnabled(false);
        item->setVisible(false);
        item->setFlag(ItemStacksBehindParent, true);
    }
    m_overlay = new DeckOverlay{this};
}

// TODO handle this via state-machine...
void DeckView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit bottomHandleSelected();
    event->ignore();
}

void DeckView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit bottomHandleChanged(event->pos().y() - boundingRect().y());
    event->ignore();
}

void DeckView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit bottomHandleReleased();
    event->ignore();
}

