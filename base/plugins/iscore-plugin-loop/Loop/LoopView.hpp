#pragma once
#include <Process/LayerView.hpp>


class LoopView final : public LayerView
{
    public:
        LoopView(QGraphicsItem* parent):
            LayerView {parent}
        {

        }

        ~LoopView()
        {

        }

        void setSelectionArea(QRectF)
        {

        }

    protected:
        void paint_impl(QPainter*) const override;
};
