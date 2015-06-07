#pragma once

#include <ProcessInterface/ProcessView.hpp>

class AutomationView : public ProcessView
{
    Q_OBJECT
    public:
        AutomationView(QGraphicsObject* parent);
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);

    signals:
        void pressed();

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent* ev);
};
