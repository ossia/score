#include <QPainter>
#include "PluginCurveZoomer.hpp"

PluginCurveZoomer::PluginCurveZoomer (QGraphicsObject* parent) :
    QGraphicsObject (parent)
{
    this->setZValue (parent->zValue() + 1);
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
    qreal fact = delta / 120.0;
    setTransformOriginPoint (origin);

    setScale (scaleFact * (1.0 + fact / 20.0) );
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
