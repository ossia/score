#pragma once
#include <ProcessInterface/LayerView.hpp>

class SpaceLayerView : public LayerView
{
        Q_OBJECT
    public:
        SpaceLayerView(QGraphicsItem* parent);

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    signals:
        void guiRequested();

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
};
