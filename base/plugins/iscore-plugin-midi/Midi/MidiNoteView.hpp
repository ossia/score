#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <Midi/MidiNote.hpp>

namespace Midi
{
class NoteView final :
        public QObject,
        public QGraphicsItem
{
        Q_OBJECT
    public:
        const Note& note;

        NoteView(const Note& n, QGraphicsItem* parent);

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

    signals:
        void noteChanged(int);

    private:
        QVariant itemChange(GraphicsItemChange change, const QVariant &value);

        double m_width{};
        double m_height{};
};
}
