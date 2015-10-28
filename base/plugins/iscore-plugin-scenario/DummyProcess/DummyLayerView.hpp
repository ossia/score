#pragma once
#include <ProcessInterface/LayerView.hpp>

class DummyLayerView : public LayerView
{
    protected:
        void paint_impl(QPainter*) const;
};
