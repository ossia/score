#pragma once
#include <Process/LayerView.hpp>

class SpaceLayerView : public LayerView
{
        Q_OBJECT
    public:
        SpaceLayerView(QGraphicsItem* parent);

        void paint_impl(QPainter *painter) const;

    signals:
        void guiRequested();

        void contextMenuRequested(const QPoint&, const QPointF&);

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
        void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;
};
