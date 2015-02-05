#include "PluginCurveMap.hpp"
#include <iostream>

PluginCurveMap::PluginCurveMap (QRectF scaleRect, QRectF paintRect, QObject* parent) :
	QObject(parent) //, _scaleRect(&scaleRect), _paintRect(&paintRect)
{
	_pScaleRect = new QRectF (scaleRect);
	_pPaintRect = new QRectF (paintRect);
}

QPointF PluginCurveMap::scaleToPaint (QPointF val)
{
	QPointF pos;
	pos.setX ( (val.x() - _pScaleRect->x() ) *_pPaintRect->width() / _pScaleRect->width() + _pPaintRect->x() );
//    pos.setY( (val.y()-_pScaleRect->y())*_pPainRect->height() / _pScaleRect->height() + _pPaintRect->y());
	pos.setY ( _pPaintRect->height() + _pPaintRect->y() - (_pPaintRect->height() * (val.y() - _pScaleRect->y() ) / _pScaleRect->height() ) );
	return pos;
}

QRectF PluginCurveMap::scaleToPaint (QRectF rect)
{
	QRectF res;
	res.setBottomLeft (scaleToPaint (rect.topLeft() ) );
	res.setBottomRight (scaleToPaint (rect.topRight() ) );
	res.setTopLeft (scaleToPaint (rect.bottomLeft() ) );
	res.setTopRight (scaleToPaint (rect.bottomRight() ) );
	return res;
}

QPointF PluginCurveMap::paintToScale (QPointF pos)
{
	QPointF val;
	val.setX ( (pos.x() - _pPaintRect->x() ) *_pScaleRect->width() / _pPaintRect->width() + _pScaleRect->x() );
	val.setY ( _pScaleRect->y() + qAbs (_pPaintRect->height() + _pPaintRect->y() - pos.y() ) / _pPaintRect->height() * _pScaleRect->height() );
	return val;
}

QRectF PluginCurveMap::paintToScale (QRectF rect)
{
	QRectF res;
	res.setBottomLeft (paintToScale (rect.topLeft() ) );
	res.setBottomRight (paintToScale (rect.topRight() ) );
	res.setTopLeft (paintToScale (rect.bottomLeft() ) );
	res.setTopRight (paintToScale (rect.bottomRight() ) );
	return res;
}

QRectF PluginCurveMap::paintRect()
{
	return *_pPaintRect;
}

QRectF PluginCurveMap::scaleRect()
{
	return *_pScaleRect;
}

void PluginCurveMap::setPaintRect (QRectF paintRect, bool changeScaleRect)
{
	//update scale rect !
	if (changeScaleRect)
	{
		setScaleRect (paintToScale (paintRect) );
	}

	*_pPaintRect = paintRect;
}

void PluginCurveMap::setScaleRect (QRectF scaleRect)
{
	*_pScaleRect = scaleRect;
	emit (mapChanged() );
}
