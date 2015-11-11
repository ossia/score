#include "SpaceLayerView.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QBrush>
#include <QPen>


SpaceLayerView::SpaceLayerView(QGraphicsItem *parent):
    LayerView{parent}
{
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable); // TODO should not be ?
    this->setZValue(100);
    this->setWidth(300);
    this->setHeight(300);
}

void SpaceLayerView::paint_impl(QPainter *painter) const
{
}

void SpaceLayerView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    emit guiRequested();
}

void SpaceLayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}
