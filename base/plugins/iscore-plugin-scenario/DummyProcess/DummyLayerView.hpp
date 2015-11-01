#pragma once
#include <Process/LayerView.hpp>

class DummyLayerView final : public LayerView
{
    public:
        explicit DummyLayerView(QGraphicsItem* parent);
    protected:
        void paint_impl(QPainter*) const override;
};
