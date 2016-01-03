#include "SpaceLayerView.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QBrush>
#include <QPen>

namespace Space
{
LayerView::LayerView(QGraphicsItem *parent):
    Process::LayerView{parent}
{
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable); // TODO should not be ?
    this->setZValue(100);
    this->setWidth(300);
    this->setHeight(300);
}

void LayerView::paint_impl(QPainter *painter) const
{
}

void LayerView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    emit guiRequested();
}

void LayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}
}
