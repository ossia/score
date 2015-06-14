#pragma once
#include <QGraphicsItem>
#include <QPen>
class DeckView;
class DeckHandle : public QGraphicsItem
{
    public:
        DeckHandle(const DeckView& deckView,
                   QGraphicsItem* parent);

        static constexpr double handleHeight()
        {
            return 3.;
        }

        const DeckView& deckView() const
        {
            return m_deckView;
        }

        QRectF boundingRect() const;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        void setWidth(qreal width);

    private:
        const DeckView& m_deckView;
        qreal m_width {};
        QPen m_pen;
};
