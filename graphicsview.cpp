#include "graphicsview.hpp"
#include "mainwindow.hpp"
#include <QMouseEvent>
#include <QPoint>
#include <QAction>
#include <QVariant>

GraphicsView::GraphicsView(QWidget *parent) :
  QGraphicsView(parent)
{
  setMouseTracking(true); /// Permits to emit mousePosition() when moving the mouse
}

void GraphicsView::mouseDragMode(QAction* action)
{
  Q_ASSERT(action->data().canConvert(QVariant::Int));
  qint32 typeDragMode = action->data().toInt();

  Q_ASSERT(typeDragMode >= QGraphicsView::NoDrag && typeDragMode <= QGraphicsView::RubberBandDrag);
  switch(typeDragMode){
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


void GraphicsView::mousePressEvent(QMouseEvent *event)
{
  //MainWindow *mainWindow = qobject_cast<MainWindow*>(this->parent());
  //Q_CHECK_PTR(mainWindow);
  //if(addItem() && (event->button() == Qt::LeftButton) && (dragMode() == QGraphicsView::NoDrag)) {
    //emit event->mouseState;
    //}

  QGraphicsView::mousePressEvent(event);

}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
  QGraphicsView::mouseReleaseEvent(event);

}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
  emit mousePosition(QPoint(event->x(), event->y()));
  QGraphicsView::mouseMoveEvent(event);
}

