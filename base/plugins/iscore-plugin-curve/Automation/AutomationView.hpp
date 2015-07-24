#pragma once

#include <ProcessInterface/LayerView.hpp>

class AutomationView : public LayerView
{
    public:
        AutomationView(QGraphicsObject* parent);
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);
};
