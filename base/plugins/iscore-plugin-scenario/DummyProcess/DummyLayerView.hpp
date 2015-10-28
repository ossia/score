#pragma once
#include <ProcessInterface/LayerView.hpp>

class DummyLayerView final : public LayerView
{
    protected:
        explicit DummyLayerView(QGraphicsItem* parent);
        void paint_impl(QPainter*) const override;
};
