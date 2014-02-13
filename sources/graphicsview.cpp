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


#include "graphicsview.hpp"
#include "mainwindow.hpp"
#include "utils.hpp"
#include <QMouseEvent>
#include <QPoint>
#include <QAction>
#include <QVariant>

GraphicsView::GraphicsView(QWidget *parent)
  : QGraphicsView(parent)
{
  setRenderHint(QPainter::Antialiasing);
  setSceneRect(QRectF()); /// we unset graphicsView sceneRect to give this ability to graphicsScene
  setMouseTracking(true); /// Permits to emit mousePosition() when moving the mouse
}

void GraphicsView::mouseDragMode(QAction* action)
{
  Q_ASSERT(action->data().canConvert(QVariant::Int));
  qint32 typeDragMode = action->data().toInt();

  Q_ASSERT(typeDragMode >= QGraphicsView::NoDrag && typeDragMode <= QGraphicsView::RubberBandDrag || typeDragMode == EventItemType || typeDragMode == BoxItemType);
  switch(typeDragMode){
    case EventItemType : // we pass Event and Process Item to NoDrag
    case BoxItemType :
    case QGraphicsView::NoDrag :
      setCursor(Qt::ArrowCursor);
      setDragMode(QGraphicsView::NoDrag);
      break;
    case QGraphicsView::ScrollHandDrag :
      setCursor(Qt::OpenHandCursor);
      setDragMode(QGraphicsView::ScrollHandDrag);
      break;
    case QGraphicsView::RubberBandDrag :
      setCursor(Qt::CrossCursor);
      setDragMode(QGraphicsView::RubberBandDrag);
      break;
    default :
      qWarning("Undefined mouse drag mode : %d", typeDragMode);
    }
}

void GraphicsView::graphicItemEnsureVisible() /// @todo Change behaviour : do a zoom animation and switch the current qGScene
{
  QGraphicsItem *item = qobject_cast<QGraphicsItem*>(sender());
  centerOn(item);
}

void GraphicsView::mousePressEvent(QMouseEvent *event)
{

  if((event->button() == Qt::LeftButton) && (dragMode() == QGraphicsView::NoDrag)) {
    emit mousePressAddItem(mapToScene(event->pos()));
  }

  QGraphicsView::mousePressEvent(event);

}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
  QGraphicsView::mouseReleaseEvent(event);

}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
  emit mousePosition(mapToScene(event->pos()));
  QGraphicsView::mouseMoveEvent(event);
}

