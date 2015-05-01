#pragma once
#include <QGraphicsItem>
class DeckView;
class DeckHandle;
class DeckOverlay : public QGraphicsItem
{
    public:
        const DeckView& deckView;
        DeckOverlay(DeckView* parent);

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
        DeckHandle* m_handle{};
};
