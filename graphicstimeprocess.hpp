#ifndef GRAPHICSTIMEPROCESS_HPP
#define GRAPHICSTIMEPROCESS_HPP

#include <QGraphicsObject>
#include "graphicstimeevent.hpp"

class GraphicsTimeProcess : public QGraphicsObject
{
  Q_OBJECT

private:
  GraphicsTimeEvent *_startTimeEvent;
  GraphicsTimeEvent *_endTimeEvent;

public:
  explicit GraphicsTimeProcess(QObject *parent = 0);

signals:

public slots:


  // QGraphicsItem interface
public:
  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  virtual int type() const;
};

#endif // GRAPHICSTIMEPROCESS_HPP
