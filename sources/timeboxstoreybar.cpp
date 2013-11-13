/*
Copyright: LaBRI / SCRIME

Authors : Jaime Chao, Clément Bossut (2013-2014)

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

#include "timeboxstoreybar.hpp"
#include <QGraphicsPixmapItem>
#include <QComboBox>
#include <QGraphicsProxyWidget>
#include <QGraphicsLinearLayout>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QPainter>


TimeboxStoreyBar::TimeboxStoreyBar(QGraphicsItem *item)
  : QGraphicsWidget(item), _height(25)
{

  setGeometry(1, parentItem()->boundingRect().height()-_height,
              parentItem()->boundingRect().width()-2, _height);
  //setMaximumWidth(parentItem()->boundingRect().width()-2); /// @todo Connect the model's members width and height to this class
  //setMaximumHeight(_height);

  _pButtonAdd = new QGraphicsPixmapItem(QPixmap(":/plus.png"), this);
  _pButtonAdd->setFlags(QGraphicsItem::ItemIgnoresTransformations); /// No need to zoom an icon
  _pButtonAdd->setPos(0, MARGIN);

  _pComboBox = new QComboBox(); /// @todo Subclass ant create model to do some extra work
  _pComboBox->setStyleSheet(
    "QComboBox {"
    "border: none;"
    "border-radius: none;"
    "};"
    );
  _pComboBoxProxy = new QGraphicsProxyWidget(this);
  _pComboBoxProxy->setWidget(_pComboBox);
  _pComboBoxProxy->setPos(size().width() -_pComboBoxProxy->size().width() - MARGIN, MARGIN);

  //qDebug() << "TimeBoxStoreyBar: " << contentsRect();
  //qDebug() << "TimeBoxStoreyBar size: " << size();
}

void TimeboxStoreyBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  /// Draw the bounding rectangle
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(Qt::gray));
  //painter->drawRect(0, 0, size().width(), size().height());
  painter->drawRect(boundingRect());//.adjusted(0,0,-1,-1));
}

QRectF TimeboxStoreyBar::boundingRect() const
{
  return QRectF(contentsRect());
}

void TimeboxStoreyBar::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
  if(event->button() == Qt::LeftButton) {
      if(_pButtonAdd->contains(mapToItem(_pButtonAdd, event->pos()))) {
          emit buttonAddClicked();
        }
    }
  QGraphicsWidget::mousePressEvent(event);
}
