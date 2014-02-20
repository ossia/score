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

#include "scenarioview.hpp"
#include <QGraphicsWidget>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QPen>
#include <QBrush>
#include <QDebug>

ScenarioView::ScenarioView(QGraphicsItem *parent)
  : PluginView(parent)
{
  //setFlags(QGraphicsItem::ItemHasNoContents);
}

void ScenarioView::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{

  //if (itemAt(mouseEvent->scenePos()) == 0) {
      if (_pTemporaryBox != NULL) {
          delete _pTemporaryBox;
          _pTemporaryBox = NULL;
        }

      // Store the first pressed point
      _pressPoint = mouseEvent->pos();

      // Add the temporary box to the scene
      _pTemporaryBox = new QGraphicsRectItem(QRectF(_pressPoint.x(), _pressPoint.y(), 0, 0), this);
      _pTemporaryBox->setPen(QPen(Qt::black));
      _pTemporaryBox->setBrush(QBrush(Qt::NoBrush));
  //  }
}

void ScenarioView::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
  //case CREATION_MODE:
   // if (noBoxSelected() || subScenarioMode(mouseEvent)) {
     //   if (resizeMode() == NO_RESIZE && _pTemporaryBox) {
  if (_pTemporaryBox != NULL) {
      int upLeftX, upLeftY, width, height;

      if (_pressPoint.x() < mouseEvent->pos().x()) {
          upLeftX = _pressPoint.x();
          width = mouseEvent->pos().x() - upLeftX;
        }
      else {
          upLeftX = mouseEvent->pos().x();
          width = _pressPoint.x() - upLeftX;
        }

      if (_pressPoint.y() < mouseEvent->pos().y()) {
          upLeftY = _pressPoint.y();
          height = mouseEvent->pos().y() - upLeftY;
        }
      else {
          upLeftY = mouseEvent->pos().y();
          height = _pressPoint.y() - upLeftY;
        }

      _pTemporaryBox->setRect(upLeftX, upLeftY, width, height);
    }
}

void ScenarioView::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
  _releasePoint = mouseEvent->pos();
    if (_pTemporaryBox != NULL) {
        if (_releasePoint != _pressPoint && (abs(_releasePoint.x() - _pressPoint.x()) > MIN_BOX_WIDTH && abs(_releasePoint.y() - _pressPoint.y()) > MIN_BOX_HEIGHT)) {

            //qDebug() << "pos :" << _pTemporaryBox->scenePos() << _pTemporaryBox;
            _pTemporaryBox->setPos(_pressPoint);

            emit addTimebox(_pTemporaryBox);
            delete _pTemporaryBox;
            _pTemporaryBox = NULL;
          }

      }
}
