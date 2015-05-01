#pragma once
#include <QGraphicsItem>
class DeckView;
class DeckHandle : public QGraphicsItem
{
    public:
        const DeckView& deckView;
        DeckHandle(const DeckView& deckView,
                   QGraphicsItem* parent);

        static constexpr double handleHeight()
        {
            return 3.;
        }

        QRectF boundingRect() const;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        void setWidth(qreal width);

    private:
        qreal m_width {};
};
