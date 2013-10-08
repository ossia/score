#include "graphicstimeprocess.hpp"

GraphicsTimeProcess::GraphicsTimeProcess(const QPoint &position, QGraphicsItem *parent, QGraphicsScene *scene)
  : QGraphicsObject(parent), _scene(scene), _width(200), _height(100)
{
  setFlags(QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemSendsGeometryChanges);

  //creer les time event de début et de fin

  setPos(position);
  _scene->clearSelection();
  _scene->addItem(this);
  setSelected(true);
}


QRectF GraphicsTimeProcess::boundingRect() const
{
  return QRectF(0,0,_width,_height);
}

void GraphicsTimeProcess::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  /// Draw the header part
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(Qt::gray));
  painter->drawRect(0,0,_width,20);

  painter->setPen(Qt::SolidLine);
  painter->setBrush(QBrush(Qt::NoBrush));
  painter->drawText(boundingRect(), Qt::AlignLeft | Qt::AlignTop, tr("Box"));

  /// Draw the bounding rectangle
  painter->drawRect(boundingRect());


}
