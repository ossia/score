#ifndef PLUGINVIEW_HPP
#define PLUGINVIEW_HPP

#include <QGraphicsObject>
#include <QRectF>
class QGraphicsItem;

class PluginView : public QGraphicsObject
{
  Q_OBJECT

private:
  QRectF _boundingRectangle;

public:
  PluginView(QGraphicsItem *parent);

public:
  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};

#endif // PLUGINVIEW_HPP
