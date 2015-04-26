#include "DeckView.hpp"

#include <iscore/tools/NamedObject.hpp>

#include <QGraphicsScene>


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
    if(!m_overlay)
        return;

    this->update();

    for(QGraphicsItem* item : childItems())
    {
        item->setEnabled(true);
        item->setVisible(true);
        item->setFlag(ItemStacksBehindParent, false);
    }

    delete m_overlay;
    m_overlay = nullptr;

    this->update();
}

void DeckView::disable()
{
    delete m_overlay;

    for(QGraphicsItem* item : childItems())
    {
        item->setEnabled(false);
        item->setVisible(false);
        item->setFlag(ItemStacksBehindParent, true);
    }

    m_overlay = new DeckOverlay{this};
}


