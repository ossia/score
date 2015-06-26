#pragma once

#include <ProcessInterface/Layer.hpp>

class AutomationView : public Layer
{
    public:
        AutomationView(QGraphicsObject* parent);
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);
};
