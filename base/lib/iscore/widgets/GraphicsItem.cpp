#include <qgraphicsitem.h>
#include <qgraphicsscene.h>

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
