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

#include "timeboxsmallview.hpp"

#include <QPainter>
#include <QGraphicsLinearLayout>
#include <QDebug>

#include "timeboxheader.hpp"
#include "timeboxstorey.hpp"
#include "timeboxmodel.hpp"


TimeboxSmallView::TimeboxSmallView(TimeboxModel *pModel, QGraphicsItem *parent)
  : QGraphicsWidget(parent), _pModel(pModel)
{
  setFlags(QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemIsSelectable);

  /// @todo Connect the model's members height and length to this class
  setGeometry(_pModel->time(), _pModel->yPosition(), _pModel->width(), 1);

  setMaximumWidth(_pModel->width()); /// Set width rigidly according to model
  setMinimumWidth(_pModel->width());


  _pLayout = new QGraphicsLinearLayout(Qt::Vertical, this);
  _pLayout->setContentsMargins(1,1,1,1);
  _pLayout->setSpacing(0);
  setLayout(_pLayout);

  _pHeader = new TimeboxHeader(this);
  _pLayout->addItem(_pHeader);  /// @bug header without layout (add margins) to avoid bad repaint of new storey
  connect(_pHeader, SIGNAL(doubleClicked()), this, SIGNAL(headerDoubleClicked()));
}

void TimeboxSmallView::addStorey(TimeboxStorey *pStorey)
{
  _pLayout->addItem(pStorey);
}

void TimeboxSmallView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  QPen pen(Qt::SolidLine);
  pen.setCosmetic(true);
  painter->setPen(pen);
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(boundingRect().adjusted(0,0,-1,-1));

  //qDebug() << "smallView: " << contentsRect() << size();
}

QRectF TimeboxSmallView::boundingRect() const
{
  return QRectF(0,0,size().width(),size().height());
}
