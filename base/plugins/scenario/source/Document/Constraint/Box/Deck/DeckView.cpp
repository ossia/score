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
}

void DeckView::setHeight(qreal height)
{
    prepareGeometryChange();
    m_height = height;
    if(m_overlay)
        m_overlay->setHeight(m_height);
}

qreal DeckView::height() const
{
    return m_height;
}

void DeckView::setWidth(qreal width)
{
    prepareGeometryChange();
    m_width = width;
    if(m_overlay)
        m_overlay->setWidth(m_width);
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
    painter->setBrush(QColor(170, 170, 170, 170));
    painter->drawRect(boundingRect());
}

void DeckOverlay::mousePressEvent(QGraphicsSceneMouseEvent *ev)
{
    ev->ignore();
}



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
