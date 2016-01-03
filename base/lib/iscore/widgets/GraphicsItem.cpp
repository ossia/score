#include <QGraphicsItem>
#include <QGraphicsScene>

#include "GraphicsItem.hpp"

void deleteGraphicsObject(QGraphicsObject* item)
{
    if(item)
    {
        auto sc = item->scene();

        if(sc)
        {
            sc->removeItem(item);
        }

        item->deleteLater();
    }
}

void deleteGraphicsItem(QGraphicsItem* item)
{
    if(item)
    {
        auto sc = item->scene();

        if(sc)
        {
            sc->removeItem(item);
        }

        delete item;
    }
}
