#include "LoopView.hpp"
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <Process/Style/ScenarioStyle.hpp>
#include <QPainter>
#include <QSlider>

namespace Loop
{
LayerView::LayerView(QGraphicsItem* parent) : Process::LayerView{parent}
{
}

LayerView::~LayerView()
{
}

void LayerView::setSelectionArea(QRectF)
{
}

void LayerView::paint_impl(QPainter* p) const
{
  p->fillRect(boundingRect(), QColor(85, 75, 0));
}

void LayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  emit askContextMenu(event->screenPos(), event->scenePos());
  event->accept();
}

void LayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  emit pressed();
  ev->accept();
}
}
