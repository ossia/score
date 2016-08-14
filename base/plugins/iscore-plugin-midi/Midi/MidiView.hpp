#pragma once
#include <Midi/MidiProcess.hpp>
#include <Process/LayerView.hpp>
#include <QPainterPath>

class QGraphicsItem;
class QPainter;

namespace Midi
{
class NoteView final :
        public QGraphicsItem
{
    public:
        const Note& note;

        NoteView(const Note& n, QGraphicsItem* parent):
            QGraphicsItem{parent},
            note{n}
        {

        }

        void setWidth(double w)
        {
            prepareGeometryChange();
            m_width = w;
        }

        void setHeight(double h)
        {
            prepareGeometryChange();
            m_height = h;
        }

        QRectF boundingRect() const override
        {
            return {0, 0, m_width, m_height};
        }

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    private:
        double m_width{};
        double m_height{};
};

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

    protected:
        void paint_impl(QPainter*) const override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
        void mousePressEvent(QGraphicsSceneMouseEvent*) override;

};
}
