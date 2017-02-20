#include <QPainter>

#include "ProgressBar.hpp"


class QWidget;

QRectF ProgressBar::boundingRect() const
{
  return {0, 0, 2, m_height};
}

void ProgressBar::paint(
    QPainter* painter)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->drawRect(boundingRect());
}

void ProgressBar::setHeight(qreal newHeight)
{
  prepareGeometryChange();
  m_height = newHeight;
}
