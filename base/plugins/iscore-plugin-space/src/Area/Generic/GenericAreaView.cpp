#include "GenericAreaView.hpp"
#include <QDebug>
#include <cmath>

namespace Space
{
GenericAreaView::GenericAreaView(QGraphicsItem *parent):
    QGraphicsItem{parent}
{
    m_col = static_cast<Qt::GlobalColor>(std::abs((double)(qrand() % 19)));
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
    painter->setPen(m_col.darker());
    painter->setBrush(m_col);

    painter->drawRects(rects);
}
}
