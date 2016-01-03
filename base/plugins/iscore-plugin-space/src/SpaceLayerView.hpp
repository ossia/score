#pragma once
#include <Process/LayerView.hpp>

namespace Space
{
class LayerView : public Process::LayerView
{
        Q_OBJECT
    public:
        LayerView(QGraphicsItem* parent);

        void paint_impl(QPainter *painter) const override;

    signals:
        void guiRequested();

        void contextMenuRequested(const QPoint&, const QPointF&);

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;
};
}
