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

#include "timeevent.hpp"

TimeEvent::TimeEvent(const QPointF &position, QGraphicsItem *parent)
  : QGraphicsObject(parent), _penWidth(1), _circleRadii(10), _height(100)
{
  setFlags(QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemSendsGeometryChanges);

  setPos(position);
}

void TimeEvent::setDate(quint32 date)
{
  _date = date;
}

QRectF TimeEvent::boundingRect() const
{
  return QRectF(-_circleRadii - _penWidth/2, -_circleRadii - _penWidth / 2,
                2*_circleRadii + _penWidth, 2*_circleRadii + _height + _penWidth);
}

void TimeEvent::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  QPen pen(Qt::SolidPattern, _penWidth);
  pen.setCosmetic(true);
  painter->setPen(pen);
  painter->drawLine(0,_circleRadii, 0, _circleRadii +_height);
  painter->drawEllipse(QPointF(0,0), _circleRadii, _circleRadii);
}

QPainterPath TimeEvent::shape() const
{
  QPainterPath path;
  path.addEllipse(QPointF(0,0), _circleRadii, _circleRadii);
  path.addRect(0,_circleRadii, _penWidth, _height); /// We can select the object 1 pixel surrounding the line
  return path;
}
