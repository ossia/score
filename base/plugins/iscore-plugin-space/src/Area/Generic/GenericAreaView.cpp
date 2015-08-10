#include "GenericAreaView.hpp"
#include <QDebug>
GenericAreaView::GenericAreaView(QGraphicsItem *parent):
    QGraphicsItem{parent}
{
}

QRectF GenericAreaView::boundingRect() const
{
    return m_rect;
}

void GenericAreaView::updateRect(const QRectF& r)
{
    prepareGeometryChange();
    m_rect = r;
}

void GenericAreaView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QColor col = Qt::GlobalColor(std::abs(qrand() % 19));
    painter->setPen(col.darker());
    painter->setBrush(col);

    painter->drawRects(rects);
}
