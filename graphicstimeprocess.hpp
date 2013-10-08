#ifndef GRAPHICSTIMEPROCESS_HPP
#define GRAPHICSTIMEPROCESS_HPP

#include <QGraphicsObject>
#include "graphicstimeevent.hpp"
#include "itemTypes.hpp"

class GraphicsTimeProcess : public QGraphicsObject
{
  Q_OBJECT

private:
  GraphicsTimeEvent *_startTimeEvent;
  GraphicsTimeEvent *_endTimeEvent;
  QGraphicsScene *_scene;
  qreal  _width;
  qreal _height;

public:
  enum {Type = ProcessItemType};

  explicit GraphicsTimeProcess(const QPoint &position, QGraphicsItem *parent, QGraphicsScene *scene);

signals:

public slots:


  // QGraphicsItem interface
public:
  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  virtual int type() const {return Type;}
};

#endif // GRAPHICSTIMEPROCESS_HPP
