#include "PluginCurveSection.hpp"
#include "PluginCurvePoint.hpp"
#include "../PluginCurveView.hpp"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>

PluginCurveSection::PluginCurveSection (QGraphicsObject* parent, PluginCurvePoint* source, PluginCurvePoint* dest) :
    QGraphicsObject (parent)
{
    _pSourcePoint = source;
    _pDestPoint = dest;
    _color = QColor (Qt::darkGray);
    _selectColor = QColor (Qt::red);
    setCacheMode (DeviceCoordinateCache);
    setFlag (ItemIgnoresTransformations);
    setFlag (ItemClipsToShape);
    setAcceptHoverEvents (true);
    setZValue (0);
    setPos (source->pos() );
}

PluginCurveSection::~PluginCurveSection()
{
}

PluginCurvePoint* PluginCurveSection::sourcePoint()
{
    return _pSourcePoint;
}

PluginCurvePoint* PluginCurveSection::destPoint()
{
    return _pDestPoint;
}

void PluginCurveSection::setSourcePoint (PluginCurvePoint* autoPoint)
{
    _pSourcePoint = autoPoint;
    adjust();
}

void PluginCurveSection::setDestPoint (PluginCurvePoint* autoPoint)
{
    _pDestPoint = autoPoint;
    adjust();
}

qreal PluginCurveSection::bendingCoef()
{
    return _coef;
}

void PluginCurveSection::setBendingCoef (qreal coef)
{
    _coef = coef;
}

void PluginCurveSection::highlight (bool b)
{
    _highlight = b;
}

QColor PluginCurveSection::color()
{
    return _color;
}

QColor PluginCurveSection::selectColor()
{
    return _selectColor;
}

void PluginCurveSection::mousePressEvent (QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::RightButton)
    {
        emit rightClicked (this, event->scenePos() );
    }
    else if (event->button() == Qt::LeftButton)
    {
        _pSourcePoint->setSelected (true);
        _pDestPoint->setSelected (true);
    }
}

void PluginCurveSection::mouseDoubleClickEvent (QGraphicsSceneMouseEvent* event)
{
    emit (doubleClicked (event) );
}

void PluginCurveSection::hoverEnterEvent (QGraphicsSceneHoverEvent* event)
{
    highlight (true);
    QGraphicsItem::hoverEnterEvent (event);
}

void PluginCurveSection::hoverLeaveEvent (QGraphicsSceneHoverEvent* event)
{
    highlight (false);
    QGraphicsItem::hoverLeaveEvent (event);
}

QVariant PluginCurveSection::itemChange (GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged)
    {
        adjust();
    }

    if (change == ItemSelectedChange)
    {

    }

    return QGraphicsItem::itemChange (change, value);
}

void PluginCurveSection::adjust()
{
    if (_pSourcePoint == NULL || _pDestPoint == NULL)
    {
        return;
    }

    setPos (_pSourcePoint->pos() );
    prepareGeometryChange();
    update();//inutile ?
}

void PluginCurveSection::setAllFlags (bool b)
{
    setFlag (QGraphicsItem::ItemIsMovable, b);
    setFlag (QGraphicsItem::ItemIsSelectable, b);
    setFlag (QGraphicsItem::ItemSendsGeometryChanges, b);
}


