#pragma once
#include <Process/LayerView.hpp>
#include <QRect>

class QGraphicsItem;
class QPainter;


class LoopView final : public LayerView
{
        Q_OBJECT
    public:
        LoopView(QGraphicsItem* parent);

        ~LoopView();

        void setSelectionArea(QRectF);

    signals:
        void askContextMenu(const QPoint&, const QPointF&);

    protected:
        void paint_impl(QPainter*) const override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
};
