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

#include "timeeventview.hpp"
#include "timeeventmodel.hpp"
#include "timeevent.hpp"

#include <QPen>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsItem>

TimeEventView::TimeEventView(TimeEventModel *pModel, TimeEvent *parentObject, QGraphicsItem *parentGraphics) :
  QGraphicsObject(parentGraphics), _pModel(pModel), _penWidth(1), _circleRadii(10), _height(0)
{
  setFlags(QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemSendsScenePositionChanges |
           QGraphicsItem::ItemSendsGeometryChanges);

  setParent(parentObject); ///@todo vérifier si ça ne pose pas problème d'avoir un parent graphique et object différents ?
  setPos(_pModel->time(), _pModel->yPosition());
  setZValue(1); // Draw on top of Timebox
  setSelected(true);

  connect(_pModel, SIGNAL(timeChanged(qreal)), this, SLOT(setX(qreal)));
  connect(_pModel, SIGNAL(yPositionChanged(qreal)), this, SLOT(setY(qreal)));
  connect(this, SIGNAL(xChanged(qreal)), _pModel, SLOT(settime(qreal)));
  connect(this, SIGNAL(yChanged(qreal)), _pModel, SLOT(setYPosition(qreal)));
}

TimeEventView::~TimeEventView()
{
}

QVariant TimeEventView::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        QPointF newPos = value.toPointF();  // value is the new position

        QRectF rect = scene()->sceneRect();
        QRectF bRectMoved = boundingRect();
        bRectMoved.moveTo(newPos);
        if (!rect.contains(bRectMoved)) { // if item exceed plugin scenario we keep the item inside the scene rect
            newPos.setX(qMin(rect.right(), qMax(newPos.x(), rect.left())));
            newPos.setY(qMin(rect.bottom(), qMax(newPos.y(), rect.top())));
            return newPos;
        }

        emit xChanged(newPos.x());  // Inform the model
        emit yChanged(newPos.y());
    }
    return QGraphicsObject::itemChange(change, value);
}
QRectF TimeEventView::boundingRect() const
{
  return QRectF(-_circleRadii - _penWidth/2, -_circleRadii - _penWidth / 2,
                2*_circleRadii + _penWidth, 2*_circleRadii + _height + _penWidth);
}

void TimeEventView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  QPen pen(Qt::SolidPattern, _penWidth);
  pen.setCosmetic(true);
  painter->setPen(pen);
  painter->drawLine(0,_circleRadii, 0, _circleRadii +_height);
  painter->drawEllipse(QPointF(0,0), _circleRadii, _circleRadii);
}

QPainterPath TimeEventView::shape() const
{
  QPainterPath path;
  path.addEllipse(QPointF(0,0), _circleRadii, _circleRadii);
  path.addRect(0,_circleRadii, _penWidth, _height); /// We can select the object 1 pixel surrounding the line
  return path;
}

void TimeEventView::setY(qreal arg)
{
  setPos(x(), arg);
}

void TimeEventView::setX(qreal arg)
{
  setPos(arg, y());
}
