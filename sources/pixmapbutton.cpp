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

#include "pixmapbutton.hpp"
#include <QRectF>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QDebug>

PixmapButton::PixmapButton(const QString &filename1, const QString &filename2, QGraphicsItem *parent)
  : QGraphicsWidget(parent), isPixmap(0)
{
  setFlags(ItemHasNoContents);
  _pButtonOne = new QGraphicsPixmapItem(QPixmap(filename1), this); /// @todo Possible to cache the same pixmap for all storeybar ?
  _pButtonOne->setFlags(QGraphicsItem::ItemIgnoresTransformations); /// No need to zoom an icon
  _pButtonOne->setCacheMode(QGraphicsItem::ItemCoordinateCache); /// @todo choose between this one and the other cache mode
  _pButtonOne->setShapeMode(QGraphicsPixmapItem::BoundingRectShape); /// Don't compute transparency to set the boundingRect
  _pButtonOne->setVisible(true);

  _pButtonTwo = new QGraphicsPixmapItem(QPixmap(filename2), this); /// @todo Possible to cache the same pixmap for all storeybar ?
  _pButtonTwo->setFlags(QGraphicsItem::ItemIgnoresTransformations); /// No need to zoom an icon
  _pButtonOne->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
  _pButtonTwo->setVisible(false);

  _pBoundingRect = new QRectF(_pButtonOne->boundingRect().united(_pButtonTwo->boundingRect()));
  //qDebug() << *_pBoundingRect;
}

void PixmapButton::setPixmap(bool id)
{
  Q_ASSERT(id == 0 || id == 1);
  if (id == 0) {
      _pButtonOne->setVisible(true);
      _pButtonTwo->setVisible(false);
      isPixmap = 0;
    }
  else if (id == 1) {
      _pButtonOne->setVisible(false);
      _pButtonTwo->setVisible(true);
      isPixmap = 1;
    }
}

QRectF PixmapButton::boundingRect() const
{
  return *_pBoundingRect;
}

void PixmapButton::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
  if(event->button() == Qt::LeftButton) {
      event->accept();
      emit clicked(isPixmap);
    }
}
