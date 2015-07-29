#pragma once
#include <ProcessInterface/LayerView.hpp>

class SpaceLayerView : public LayerView
{
    public:
        SpaceLayerView(QGraphicsItem* parent):
            LayerView{parent}
        {
            this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable); // TODO should not be ?
            this->setZValue(parent->zValue() + 1);
        }

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};
