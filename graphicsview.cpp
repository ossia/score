#include "graphicsview.hpp"
#include <QMouseEvent>
#include <QPoint>

GraphicsView::GraphicsView(QWidget *parent) :
  QGraphicsView(parent)
{
  setMouseTracking(true);
}


void GraphicsView::mousePressEvent(QMouseEvent *event)
{
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

