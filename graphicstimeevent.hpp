/*! @file
 *  @brief Graphical representation of a TTTimeEvent class.
 *  @author Jaime Chao
 */

#ifndef GRAPHICSTIMEEVENT_HPP
#define GRAPHICSTIMEEVENT_HPP

#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QPainter>
#include <QDate>

#include"itemTypes.hpp"

class GraphicsTimeEvent : public QGraphicsObject
{
  Q_OBJECT
  Q_PROPERTY(QDate date READ date WRITE setDate) /// \todo Unification avec la date de TTTimeEvent

public:
  enum {Type = EventItemType};

  explicit GraphicsTimeEvent(const QPointF &position, QGraphicsItem *parent, QGraphicsScene *scene);
  QDate date() const { return _date; }

public slots:
  void setDate(QDate date);

signals:
  void dirty();

private:
  QGraphicsScene *_scene;
  qreal _penWidth;
  qreal _circleRadii; /// a straight line from the centre to the circumference of the bottom circle
  qreal _height; /// height of the line
  QDate _date;

  // QGraphicsItem interface
public:
  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  virtual int type() const {return Type;}
  virtual QPainterPath shape() const;

protected:
  virtual void keyPressEvent(QKeyEvent *event);
  virtual void keyReleaseEvent(QKeyEvent *event);
  virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
  virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
  virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
  virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
  virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);

};

#endif // GRAPHICSTIMEEVENT_HPP
