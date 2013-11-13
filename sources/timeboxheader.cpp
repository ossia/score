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

#include "timeboxheader.hpp"
#include <QPainter>
#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsSimpleTextItem>

TimeboxHeader::TimeboxHeader(QGraphicsItem *item)
  : QGraphicsWidget(item)
{
  setGeometry(0,0, parentItem()->boundingRect().width(), HEIGHT);

  setMaximumHeight(HEIGHT); /// Set height rigidly
  setMinimumHeight(HEIGHT);

  _pButtonPlay = new QGraphicsPixmapItem(QPixmap(":/play.png"), this);
  _pButtonPlay->setFlags(QGraphicsItem::ItemIgnoresTransformations); /// No need to zoom an icon
  _pButtonPlay->setPos(0, MARGIN);

  _pTextName = new QGraphicsSimpleTextItem(tr("Box"), this);
  _pTextName->setFlags(QGraphicsItem::ItemIgnoresTransformations); /// No need to zoom a text
  _pTextName->setPos(30, MARGIN); /// @todo Find a better position
}

void TimeboxHeader::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  /// Draw the header part
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(Qt::gray));
  painter->drawRect(contentsRect());

  qDebug() << "header: " << contentsRect() << size();
}

QRectF TimeboxHeader::boundingRect() const
{
  return QRectF(0,0,size().width(),size().height());
}


void TimeboxHeader::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
}
