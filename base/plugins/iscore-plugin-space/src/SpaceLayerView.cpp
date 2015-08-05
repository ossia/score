#include "SpaceLayerView.hpp"
#include <QPainter>
#include <QBrush>
#include <QPen>

SpaceLayerView::SpaceLayerView(QGraphicsItem *parent):
    LayerView{parent}
{
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable); // TODO should not be ?
    this->setZValue(parent->zValue() + 1);
}

void SpaceLayerView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
}

void SpaceLayerView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    emit guiRequested();
}
