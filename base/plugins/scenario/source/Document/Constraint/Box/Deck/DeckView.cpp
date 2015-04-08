#include "DeckView.hpp"

#include <iscore/tools/NamedObject.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

class DeckOverlay : public QGraphicsItem
{
        DeckView* m_view{};
    public:
        DeckOverlay(DeckView* parent):
            QGraphicsItem{parent},
            m_view{parent}
        {
            this->setZValue(1000);
            this->setPos(0, 0);

        }
        virtual QRectF boundingRect() const override
        {
            return m_view->boundingRect();
        }
        virtual void paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option, QWidget *widget) override
        {
            painter->setBrush(QColor(200, 200, 200, 200));
            painter->drawRect(boundingRect());
        }
};

DeckView::DeckView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 1);
}

QRectF DeckView::boundingRect() const
{
    return {0, 0,
            qreal(m_width),
            qreal(m_height)
           };
}

void DeckView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    auto rect = boundingRect();
    painter->drawRect(rect);
    painter->setBrush(QBrush {Qt::lightGray});

    painter->drawRect(0, rect.height() - borderHeight(), rect.width(), borderHeight());
}

void DeckView::setHeight(int height)
{
    prepareGeometryChange();
    m_height = height;
}

int DeckView::height() const
{
    return m_height;
}

void DeckView::setWidth(int width)
{
    prepareGeometryChange();
    m_width = width;
}

int DeckView::width() const
{
    return m_width;
}

void DeckView::enable()
{
    delete m_overlay;for(QGraphicsItem* item : childItems())
    {
        item->setEnabled(true);
    }
}

void DeckView::disable()
{
    m_overlay = new DeckOverlay{this};
    for(QGraphicsItem* item : childItems())
    {
        item->setEnabled(false);
    }
}

void DeckView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit bottomHandleSelected();
}

void DeckView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit bottomHandleChanged(event->pos().y() - boundingRect().y());
}

void DeckView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit bottomHandleReleased();
}

