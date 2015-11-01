#include "ProgressBar.hpp"
#include <QPainter>
QRectF ProgressBar::boundingRect() const
{
    return {0, 0, 2, m_height};
}


void ProgressBar::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    painter->drawRect(boundingRect());
}

void ProgressBar::setHeight(qreal newHeight)
{
    prepareGeometryChange();
    m_height = newHeight;
}
