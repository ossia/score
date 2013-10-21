#include "graphicsview.hpp"
#include "mainwindow.hpp"
#include "itemTypes.hpp"
#include <QMouseEvent>
#include <QPoint>
#include <QAction>
#include <QVariant>

GraphicsView::GraphicsView(QWidget *parent)
  : QGraphicsView(parent)
{
  setMouseTracking(true); /// Permits to emit mousePosition() when moving the mouse
}

void GraphicsView::mouseDragMode(QAction* action)
{
  Q_ASSERT(action->data().canConvert(QVariant::Int));
  qint32 typeDragMode = action->data().toInt();

  Q_ASSERT(typeDragMode >= QGraphicsView::NoDrag && typeDragMode <= QGraphicsView::RubberBandDrag || typeDragMode == EventItemType || typeDragMode == ProcessItemType);
  switch(typeDragMode){
    case EventItemType : // we pass Event and Process Item to NoDrag
    case ProcessItemType :
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

void GraphicsView::graphicItemEnsureVisible()
{
  QGraphicsItem *item = qobject_cast<QGraphicsItem*>(sender());
  //centerOn(item);
  fitInView(item, Qt::KeepAspectRatio); /// Automatically resize and center the item, Qt::AspectRatioMode is not ignored
  //QGraphicsView::AnchorViewCenter
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

