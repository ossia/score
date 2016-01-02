#include "LoopView.hpp"
#include <QGraphicsSceneContextMenuEvent>
class QPainter;

namespace Loop
{
LayerView::LayerView(QGraphicsItem *parent):
    Process::LayerView {parent}
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

void LayerView::contextMenuEvent(
        QGraphicsSceneContextMenuEvent* event)
{
    emit askContextMenu(event->screenPos(), event->scenePos());
}
}
