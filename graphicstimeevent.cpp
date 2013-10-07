#include "graphicstimeevent.hpp"

GraphicsTimeEvent::GraphicsTimeEvent(const QPoint &position, QGraphicsItem *parent, QGraphicsScene *scene)
  : QGraphicsObject(parent), _scene(scene), _penWidth(3), _circleRadii(10), _height(100)
{
  setFlags(QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemSendsGeometryChanges);

 // qDebug("visible %d", isVisible());

  setPos(position);
  _scene->clearSelection();
  _scene->addItem(this);
  setSelected(true);
}

void GraphicsTimeEvent::setDate(QDate date)
{
  _date = date;
}

QRectF GraphicsTimeEvent::boundingRect() const
{
  return QRectF(-_circleRadii - _penWidth/2, -_circleRadii - _penWidth / 2,
                2*_circleRadii + _penWidth, 2*_circleRadii + _height + _penWidth);
}

void GraphicsTimeEvent::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

  QPen pen(Qt::SolidPattern, _penWidth);
  painter->setPen(pen);
  painter->drawLine(0,_circleRadii, 0, _circleRadii +_height);
  painter->drawEllipse(QPointF(0,0), _circleRadii, _circleRadii);
}

QPainterPath GraphicsTimeEvent::shape() const
{
  QPainterPath path;
  path.addEllipse(QPointF(0,0), _circleRadii, _circleRadii);
  path.addRect(0,_circleRadii, _penWidth, _height); /// We can select the object 1 pixel surrounding the line
  return path;
}

void GraphicsTimeEvent::keyPressEvent(QKeyEvent *event)
{
  QGraphicsObject::keyPressEvent(event);
}

void GraphicsTimeEvent::keyReleaseEvent(QKeyEvent *event)
{
}

void GraphicsTimeEvent::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
  QGraphicsObject::mousePressEvent(event);
}

void GraphicsTimeEvent::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
  QGraphicsObject::mouseMoveEvent(event);
}

void GraphicsTimeEvent::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
  QGraphicsObject::mouseReleaseEvent(event);
}

void GraphicsTimeEvent::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
}

QVariant GraphicsTimeEvent::itemChange(GraphicsItemChange change, const QVariant &value)
{
  return QGraphicsObject::itemChange(change, value);
}
