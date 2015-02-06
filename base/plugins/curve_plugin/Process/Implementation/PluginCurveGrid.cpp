#include "PluginCurveGrid.hpp"
#include "PluginCurveMap.hpp"
#include "../PluginCurveView.hpp"
#include <QPainter>
#include <iostream>
#include <cmath>

PluginCurveGrid::PluginCurveGrid (QGraphicsObject* parent, PluginCurveMap* map) :
	QGraphicsObject (parent)
{
	setObjectName("PluginCurveGrid");
	//setFlag(ItemIgnoresTransformations);

	if (map == nullptr)
	{
		_pMap = new PluginCurveMap (QRectF (0, 0, 100, 100), QRectF (0, 0, 3, 3), this);
	}
	else
	{
		_pMap = map;
		_pMap->setParent(this);
	}

	connect (_pMap, SIGNAL (mapChanged() ), this, SLOT (mapChanged() ) );
	updateMagnetPoints();
	setZValue (-1);
}

QPointF PluginCurveGrid::nearestMagnetPoint (QPointF p)
{
	QPointF res = _pMap->paintToScale (p);
	qreal x, y;
	x = floor (res.x() / stepX) * stepX;
	y = floor (res.y() / stepY) * stepY;

	if (res.x() - x > stepX / 2)
	{
		res.setX (x + stepX);
	}
	else
	{
		res.setX (x);
	}

	if (res.y() - y > stepY / 2)
	{
		res.setY (y + stepY);
	}
	else
	{
		res.setY (y);
	}

	return _pMap->scaleToPaint (res);
}

void PluginCurveGrid::paint (QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED (option)
	Q_UNUSED (widget)
	painter->setPen (Qt::lightGray);
	qreal x, y;
	QRectF paintRect = _pMap->paintRect();
	QVector<qreal>::iterator iteratorX;
	QVector<qreal>::iterator iteratorY;

	for (iteratorX = magnetPointX.begin(); iteratorX != magnetPointX.end(); iteratorX++)
	{
		x = *iteratorX;
		painter->drawLine (QLineF (x, paintRect.y(), x, paintRect.y() + paintRect.height() ) );
	}

	for (iteratorY = magnetPointY.begin(); iteratorY != magnetPointY.end(); iteratorY++)
	{
		y = *iteratorY;
		painter->drawLine (QLineF (paintRect.x(), y, paintRect.x() + paintRect.width(), y) );
	}
}


QRectF PluginCurveGrid::boundingRect() const
{
	return parentItem()->boundingRect();
}

#include <QDebug>
void PluginCurveGrid::updateMagnetPoints()
{
	magnetPointX.clear();
	magnetPointY.clear();
	qreal x, y;
	QRectF scaleRect = _pMap->scaleRect();
	QRectF paintRect = _pMap->paintRect();
	stepY = pow (10, floor (log10f (qAbs (scaleRect.height() ) ) ) );
	stepX = pow (10, floor (log10f (qAbs (scaleRect.width() ) ) ) );
	QPointF firstPoint = _pMap->scaleToPaint (QPointF (floor (scaleRect.x() / stepX + 1) * (stepX), floor (scaleRect.y() / stepY + 1) * (stepY) ) );
	qreal paintStepY = qAbs (_pMap->scaleToPaint (QPointF (0, stepY) ).y() - _pMap->scaleToPaint (QPointF (0, 0) ).y() );
	qreal paintStepX = _pMap->scaleToPaint (QPointF (stepX, 0) ).x() - _pMap->scaleToPaint (QPointF (0, 0) ).x();

	for (x = firstPoint.x(); x <= paintRect.x() + paintRect.width(); x += paintStepX)
	{
		magnetPointX.append (x);
	}

	if(paintStepY > 0)
	{
		for (y = firstPoint.y(); y >= paintRect.y(); y -= paintStepY)
		{
			magnetPointY.append (y);
		}
	}
}

void PluginCurveGrid::mapChanged()
{
	updateMagnetPoints();
	update();
}
