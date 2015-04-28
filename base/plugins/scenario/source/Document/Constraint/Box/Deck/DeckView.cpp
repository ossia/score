#include "DeckView.hpp"

#include <iscore/tools/NamedObject.hpp>

#include <QGraphicsScene>
#include <QApplication>

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
    QPalette palette{QApplication::palette()};

    painter->setPen(QPen{m_focus? palette.highlight().color() : Qt::black});
    painter->drawRect(rect);

    painter->setBrush(palette.midlight());
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

void DeckView::setFocus(bool b)
{
    m_focus = b;
    update();
}




QRectF DeckOverlay::boundingRect() const
{
    const auto& rect = deckView.boundingRect();
    return {0, 0, rect.width(), rect.height() - deckView.handleHeight()};
}

void DeckOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPalette palette{QApplication::palette()};

    painter->setPen(Qt::black);
    painter->setBrush(QColor(200, 200, 200, 200));
    painter->drawRect(boundingRect());
}

void DeckOverlay::mousePressEvent(QGraphicsSceneMouseEvent *ev)
{
    ev->ignore();
}
