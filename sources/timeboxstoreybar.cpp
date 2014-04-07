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

#include "timeboxstoreybar.hpp"
#include <QGraphicsPixmapItem>
#include <QComboBox>
#include <QGraphicsProxyWidget>
#include <QGraphicsLinearLayout>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QPainter>
#include "pixmapbutton.hpp"

TimeboxStoreyBar::TimeboxStoreyBar(QGraphicsItem *item)
  : QGraphicsWidget(item)
{
  setGeometry(0, parentItem()->boundingRect().height() - HEIGHT,
              parentItem()->boundingRect().width() - 2, HEIGHT); /// -2 to fit in storey (because storeybar is not managed by graphicslayout)

  qDebug() << "timeboxstoreybar constructor:" << parentItem()->boundingRect();

  _pButton = new PixmapButton(QString(":/plus.png"), QString(":/minus.png"), this);
  _pButton->setPos(MARGIN, MARGIN);
  connect(_pButton, SIGNAL(clicked(bool)), this, SIGNAL(buttonClicked(bool)));

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

}

void TimeboxStoreyBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  /// Draw the bounding rectangle
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(Qt::gray));

  painter->drawRect(boundingRect());//.adjusted(0,0,-1,-1));

  qDebug() << "timeboxstoreybar :" << contentsRect() << size();
}

QRectF TimeboxStoreyBar::boundingRect() const
{
  return QRectF(0,0,size().width(),size().height());
}

void TimeboxStoreyBar::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
  QGraphicsWidget::mousePressEvent(event);
}
