#pragma once

#include <ProcessInterface/LayerView.hpp>

class AutomationView : public LayerView
{
    public:
        explicit AutomationView(QGraphicsItem *parent);
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);
};
