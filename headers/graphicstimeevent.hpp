/*
Copyright: LaBRI / SCRIME

Author: Jaime Chao (01/10/2013)

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software.  You can  use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and,  more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/

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

private:
  QGraphicsScene *_scene;
  qreal _penWidth;
  qreal _circleRadii; /// a straight line from the centre to the circumference of the bottom circle
  qreal _height; /// height of the line
  QDate _date;

public:
  explicit GraphicsTimeEvent(const QPointF &position, QGraphicsItem *parent, QGraphicsScene *scene);

  enum {Type = EventItemType}; //! Type value for custom item. Enable the use of qgraphicsitem_cast with this item
  virtual int type() const {return Type;}

signals:
  void dirty();

public slots:
  void setDate(QDate date);

public:
  QDate date() const { return _date; }
  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
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
