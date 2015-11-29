#pragma once
#include <Process/LayerView.hpp>
#include <qrect.h>

class QGraphicsItem;
class QPainter;


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
