#pragma once
#include <QGraphicsItem>
class SlotView;
class SlotHandle;
class SlotOverlay : public QGraphicsItem
{
    public:
        SlotOverlay(SlotView* parent);

        const SlotView& slotView() const
        { return m_slotView; }

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
        const SlotView&m_slotView;
};
