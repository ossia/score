#include "GraphicsItem.hpp"
#include <QGraphicsScene>
#include <QGraphicsItem>

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
