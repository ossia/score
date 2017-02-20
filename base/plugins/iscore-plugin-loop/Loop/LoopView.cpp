#include "LoopView.hpp"
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
class QPainter;

namespace Loop
{
LayerView::LayerView(QQuickPaintedItem* parent) : Process::LayerView{parent}
{
}

LayerView::~LayerView()
{
}

void LayerView::setSelectionArea(QRectF)
{
}

void LayerView::paint_impl(QPainter*) const
{
}
/*
void LayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  emit askContextMenu(event->screenPos(), mapToScene(event->localPos()));
  event->accept();
}
*/
void LayerView::mousePressEvent(QMouseEvent* ev)
{
  emit pressed();
  ev->accept();
}
}
