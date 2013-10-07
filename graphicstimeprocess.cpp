#include "graphicstimeprocess.hpp"

GraphicsTimeProcess::GraphicsTimeProcess(QObject *parent) :
  QGraphicsObject(parent)
{
  //creer les time event de début et de fin
  setFlags(ItemIsSelectable ||
           ItemIsMovable);

}


QRectF GraphicsTimeProcess::boundingRect() const
{
  return QRectF(0,0,200,100);
}

void GraphicsTimeProcess::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  painter->drawRect(boundingRect());
}
