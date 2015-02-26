#include "PluginCurvePoint.hpp"
#include "PluginCurveSection.hpp"

#include "../PluginCurveModel.hpp"
#include "../PluginCurvePresenter.hpp"
#include <QPainter>
#include <QRectF>
#include <QGraphicsObject>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItem>
#include <QKeyEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <iostream>
#include <QDebug>
PluginCurvePoint::PluginCurvePoint(QGraphicsObject* parent, PluginCurvePresenter* presenter, QPointF point, QPointF value, MobilityMode mobility, bool removable) :
    QGraphicsObject(parent), _pPresenter(presenter)
{
    _color = Qt::gray; // Point's color
    _selectColor = Qt::red; // Point's color when selected
    setRemovable(removable);
    setAcceptHoverEvents(true);
    setCacheMode(DeviceCoordinateCache);
    setFlag(ItemIsFocusable, false);
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    setZValue(parent->zValue() + 10);
    setValue(value);
    setPos(point);
    setMobility(mobility);  // Warning ! setMobility after setPos();
}

void PluginCurvePoint::setValue(QPointF value)
{
    _value = value;
}

QPointF PluginCurvePoint::getValue()
{
    return _value;
}

MobilityMode PluginCurvePoint::mobility()
{
    return _mobility;
}

void PluginCurvePoint::setMobility(MobilityMode mode)
{
    _mobility = mode;

    if(mode == Vertical)
    {
        _fixedCoordinate = pos().x();
    }
}

qreal PluginCurvePoint::fixedCoordinate()
{
    return _fixedCoordinate;
}

void PluginCurvePoint::highlight(bool b = true)
{
    _highlight = b;
}

QColor PluginCurvePoint::color()
{
    return _color;
}

QColor PluginCurvePoint::selectColor()
{
    return _selectColor;
}

QPointF PluginCurvePoint::globalPos()
{
    Q_ASSERT(scene() != NULL);  // the focus item belongs to a scene
    Q_ASSERT(!scene()->views().isEmpty());   // that scene is displayed in a view...
    // TODO jm : this views().first() will bite us
    Q_ASSERT(scene()->views().first() != NULL);  // ... which is not null...
    Q_ASSERT(scene()->views().first()->viewport() != NULL);  // ... and has a viewport
    QGraphicsView* v = scene()->views().first();
    QPointF sceneP = scenePos();
    QPoint viewP = v->mapFromScene(sceneP);
    return v->viewport()->mapToGlobal(viewP);
}

bool PluginCurvePoint::removable()
{
    return _removable;
}

void PluginCurvePoint::setRemovable(bool b)
{
    _removable = b;
}

void PluginCurvePoint::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    QColor stdColor = color();
    QColor slctColor = selectColor();
    QRadialGradient gradientExt(0, 0, SHAPERADIUS);
    QRadialGradient gradientNoSelection(2, 2, SHAPERADIUS);
    QRadialGradient gradientSelection(2, 2, SHAPERADIUS);
    gradientExt.setColorAt(RADIUS / SHAPERADIUS, QColor(slctColor).light(120));
    gradientExt.setColorAt(0.8, Qt::transparent);
    gradientNoSelection.setColorAt(0, QColor(stdColor).light(100));
    gradientNoSelection.setColorAt(1, Qt::black);
    gradientSelection.setColorAt(0, QColor(slctColor).light(120));
    gradientSelection.setColorAt(1, Qt::black);

    if(_highlight)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(gradientExt);
        painter->drawEllipse(boundingRect());
    }

    if(isSelected())
    {
        painter->setBrush(gradientSelection);
    }
    else
    {
        painter->setBrush(gradientNoSelection);
    }

    //painter->setPen(Qt::NoPen);
    painter->setPen(QPen(Qt::black, 0));
    painter->drawEllipse(-RADIUS, -RADIUS, 2 * RADIUS, 2 * RADIUS);


}

void PluginCurvePoint::setRightSection(PluginCurveSection* section)
{
    _pRightSection = section;
}

void PluginCurvePoint::setLeftSection(PluginCurveSection* section)
{
    _pLeftSection = section;
}

PluginCurveSection* PluginCurvePoint::rightSection()
{
    return _pRightSection;
}

PluginCurveSection* PluginCurvePoint::leftSection()
{
    return _pLeftSection;
}

void PluginCurvePoint::adjust()
{
    PluginCurveSection* lSection = leftSection();
    PluginCurveSection* rSection = rightSection();

    if(lSection != nullptr)
    {
        lSection->adjust();
    }

    if(rSection != nullptr)
    {
        rSection->adjust();
    }
}

bool PluginCurvePoint::compareXSup(const QPointF& other) const
{
    return this->x() >= other.x();
}

bool PluginCurvePoint::compareXInf(const QPointF& other) const
{
    return this->x() <= other.x();
}

bool PluginCurvePoint::compareYSup(const QPointF& other) const
{
    return this->y() >= other.y();
}

bool PluginCurvePoint::compareYInf(const QPointF& other) const
{
    return this->y() <= other.y();
}

bool PluginCurvePoint::operator>= (const QPointF& other) const
{
    return (this->x() >= other.x());
}

bool PluginCurvePoint::operator<= (const QPointF& other) const
{
    return (this->x() <= other.x());
}

QRectF PluginCurvePoint::boundingRect() const
{
    return QRectF(-SHAPERADIUS, -SHAPERADIUS, 2 * SHAPERADIUS, 2 * SHAPERADIUS);
}

QPainterPath PluginCurvePoint::shape() const
{
    QPainterPath circle;
    circle.addEllipse(boundingRect());
    return circle;
}

#include <QDebug>
void PluginCurvePoint::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::RightButton)
    {
        emit(rightClicked(this));
    }
    else
    {
        QGraphicsItem::mousePressEvent(event);
    }
}

void PluginCurvePoint::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->buttonDownScenePos(Qt::LeftButton) != event->scenePos())    // If the point has been moved.
    {
        emit(pointPositionHasChanged());
    }

    QGraphicsItem::mouseReleaseEvent(event);
}

void PluginCurvePoint::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsItem::mouseMoveEvent(event);
}

void PluginCurvePoint::keyPressEvent(QKeyEvent* event)
{
    QGraphicsItem::keyPressEvent(event);
}

void PluginCurvePoint::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    highlight();
    QGraphicsItem::hoverEnterEvent(event);
}

void PluginCurvePoint::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    highlight(false);
    QGraphicsItem::hoverLeaveEvent(event);
}

QVariant PluginCurvePoint::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if(change == ItemPositionHasChanged)
    {
        // The point moved, the sections must be adjusted.
        adjust();
        //emit (pointPositionIsChanging(this)); //MODIFICATION
    }

    if(change == ItemPositionChange)
    {
        QPointF newPos = value.toPointF();
        _pPresenter->adjustPoint(this, newPos);
        return QVariant(newPos);
        //emit (pointPositionIsChanging(this));
        // emit (pointSelectedChange(this));
        // update();
    }

    return QGraphicsItem::itemChange(change, value);
}

void PluginCurvePoint::setAllFlags(bool b)
{
    setFlag(QGraphicsItem::ItemIsMovable, b);
    setFlag(QGraphicsItem::ItemIsSelectable, b);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, b);
}
