#pragma once
#include <Process/LayerView.hpp>
#include <QRect>

class QGraphicsItem;
class QPainter;

namespace Loop
{
class LayerView final : public Process::LayerView
{
        Q_OBJECT
    public:
        LayerView(QGraphicsItem* parent);

        ~LayerView();

        void setSelectionArea(QRectF);

    signals:
        void askContextMenu(const QPoint&, const QPointF&);
        void pressed();

    protected:
        void paint_impl(QPainter*) const override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
        void mousePressEvent(QGraphicsSceneMouseEvent*) override;
};
}
