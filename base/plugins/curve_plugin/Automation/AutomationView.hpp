#pragma once

#include <ProcessInterface/ProcessViewInterface.hpp>

class AutomationView : public ProcessViewInterface
{
    Q_OBJECT
    public:
        AutomationView(QGraphicsObject* parent);
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);

    signals:
        void mousePressed();

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent* ev)
        {
            emit mousePressed();
        }
};
