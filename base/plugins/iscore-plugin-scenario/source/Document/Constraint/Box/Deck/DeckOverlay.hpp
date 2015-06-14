#pragma once
#include <QGraphicsItem>
class DeckView;
class DeckHandle;
class DeckOverlay : public QGraphicsItem
{
    public:
        DeckOverlay(DeckView* parent);

        const DeckView& deckView() const
        { return m_deckView; }

        virtual QRectF boundingRect() const override;

        void setHeight(qreal height);
        void setWidth(qreal height);

        virtual void paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget) override;

        virtual void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        const DeckView&m_deckView;
};
