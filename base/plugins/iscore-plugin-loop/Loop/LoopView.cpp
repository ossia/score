#include "LoopView.hpp"
#include <QGraphicsSceneContextMenuEvent>
class QPainter;

LoopView::LoopView(QGraphicsItem *parent):
    LayerView {parent}
{

}

LoopView::~LoopView()
{

}

void LoopView::setSelectionArea(QRectF)
{

}

void LoopView::paint_impl(QPainter*) const
{
}

void LoopView::contextMenuEvent(
        QGraphicsSceneContextMenuEvent* event)
{
    emit askContextMenu(event->screenPos(), event->scenePos());
}
