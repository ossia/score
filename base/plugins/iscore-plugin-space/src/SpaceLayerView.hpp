#pragma once
#include <ProcessInterface/LayerView.hpp>

class SpaceLayerView : public LayerView
{
    public:
        using LayerView::LayerView;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};
