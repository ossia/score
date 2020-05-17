// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LoopView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Loop::LayerView)
namespace Loop
{
LayerView::LayerView(QGraphicsItem* parent) : Process::LayerView{parent} { }

LayerView::~LayerView() { }

void LayerView::setSelectionArea(QRectF) { }

void LayerView::paint_impl(QPainter* p) const
{
  // QColor(85, 75, 0, 200)
  auto& style = Process::Style::instance();
  p->fillRect(boundingRect(), style.LoopBrush());
}

void LayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  askContextMenu(event->screenPos(), event->scenePos());
  event->accept();
}

void LayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  pressed(ev->scenePos());
  ev->accept();
}
}
