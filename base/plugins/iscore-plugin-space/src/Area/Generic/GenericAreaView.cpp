#include "GenericAreaView.hpp"
#include <QDebug>
#include <cmath>
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
    QColor col = static_cast<Qt::GlobalColor>(std::abs((double)(qrand() % 19)));
    painter->setPen(col.darker());
    painter->setBrush(col);

    painter->drawRects(rects);
}
