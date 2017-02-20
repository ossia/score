#include <QQuickPaintedItem>
#include <QGraphicsScene>

#include "GraphicsItem.hpp"

void deleteGraphicsObject(QQuickPaintedItem* item)
{
  if (item)
  {
    item->deleteLater();
  }
}

void deleteGraphicsItem(QQuickPaintedItem* item)
{
  if (item)
  {
    item->deleteLater();
  }
}
