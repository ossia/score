#include <QPainter>
#include "plugincurvezoomer.hpp"

PluginCurveZoomer::PluginCurveZoomer (QGraphicsObject* parent) :
	QGraphicsObject (parent)
{
}

void PluginCurveZoomer::paint (QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED (painter);
	Q_UNUSED (option);
	Q_UNUSED (widget);
}

QRectF PluginCurveZoomer::boundingRect() const
{
	return mapRectFromParent (parentItem()->boundingRect() );
}

void PluginCurveZoomer::zoom (QPointF origin, qreal delta)
{
	update();
	//origin : zommer coordinate
	qreal scaleFact = scale();
	qreal fact = delta / 120;
	setTransformOriginPoint (origin);
//    setScale(qMax((fact/20)+scaleFact,0.1));
	setScale (scaleFact * (1 + fact / 20) );
	prepareGeometryChange();
	update();
}

void PluginCurveZoomer::translateX (qreal dx)
{
	setX (pos().x() + dx);
}

void PluginCurveZoomer::translateY (qreal dy)
{
	setY (pos().y() + dy);
}
