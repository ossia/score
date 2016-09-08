#pragma once
#include <Midi/MidiProcess.hpp>
#include <Process/LayerView.hpp>
#include <QPainterPath>

namespace Midi
{
class NoteView;
class View final :
        public Process::LayerView
{
        Q_OBJECT
    public:
        View(QGraphicsItem* parent);

        ~View();

    signals:
        void askContextMenu(const QPoint&, const QPointF&);
        void pressed();
        void doubleClicked(QPointF);

    protected:
        void paint_impl(QPainter*) const override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
        void mousePressEvent(QGraphicsSceneMouseEvent*) override;
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override;
};

NoteData noteAtPos(QPointF point, const QRectF& rect);
}
