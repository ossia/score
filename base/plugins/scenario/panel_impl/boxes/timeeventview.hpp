/*
Copyright: LaBRI / SCRIME

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

#ifndef TIMEEVENTVIEW_HPP
#define TIMEEVENTVIEW_HPP

class TimeEventModel;
class TimeEvent;
class QGraphicsLineItem;

#include "utils.hpp"
#include <QGraphicsObject>
#include <QLineF>

/*!
 *  This class is the graphical representation of a TimeEvent.
 *
 *  @brief TimeEvent view
 *  @author Jaime Chao
 *  @date 2014
 */

class TimeEventView : public QGraphicsObject
{
  Q_OBJECT

private:
  TimeEventModel *_pModel = nullptr;

  qreal _penWidth;
  qreal _circleRadii; /// a straight line from the centre to the circumference of the bottom circle
  qreal _height; /// height of the line

  QGraphicsLineItem *_pTemporaryRelation = nullptr; /// Temporary graphical line when a creation is in progress. Line is horizontal and always attached at the center of the circle (0,0)

public:
  explicit TimeEventView(TimeEventModel *pModel, TimeEvent *parentObject, QGraphicsItem *parentGraphics = 0);
  ~TimeEventView();

signals:
  void xChanged(qreal);
  void yChanged(qreal);
  void createTimeEventAndTimebox(QLineF line);  /// emit a signal to create a Timebox and another TimeEvent in the current Scenario

public slots:
  void setY(qreal);
  void setX(qreal);

public:
  enum {Type = EventItemType}; //! Type value for custom item. Enable the use of qgraphicsitem_cast with this item
  virtual int type() const {return Type;}

  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  virtual QPainterPath shape() const;

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant &value);
  void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
  void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);

};

#endif // TIMEEVENTVIEW_HPP
