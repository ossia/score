#include "AreaView.hpp"
#include <QDebug>
AreaView::AreaView(QGraphicsItem *parent):
    QGraphicsItem{parent}
{
}

QRectF AreaView::boundingRect() const
{
    return m_rect;
}

void AreaView::updateRect(const QRectF& r)
{
    prepareGeometryChange();
    m_rect = r;
}

void AreaView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QColor col = Qt::GlobalColor(std::abs(qrand() % 19));
    painter->setPen(col.darker());
    painter->setBrush(col);

    painter->drawRects(rects);
}
