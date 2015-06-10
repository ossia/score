#pragma once

#include <ProcessInterface/ProcessView.hpp>

class AutomationView : public ProcessView
{
    public:
        AutomationView(QGraphicsObject* parent);
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);
};
