#include "AreaView.hpp"
AreaView::AreaView(QGraphicsItem *parent)
{

}

QRectF AreaView::boundingRect() const
{
    return {0, 0, 800, 600};
}

void AreaView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QColor col = Qt::green;
    painter->setPen(col.darker());
    painter->setBrush(col);

    painter->drawRects(rects);
}
