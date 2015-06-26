#pragma once
#include <QGraphicsItem>
#include <QPen>
class SlotView;
class SlotHandle : public QGraphicsItem
{
    public:
        SlotHandle(const SlotView& slotView,
                   QGraphicsItem* parent);

        static constexpr double handleHeight()
        {
            return 3.;
        }

        const SlotView& slotView() const
        {
            return m_slotView;
        }

        QRectF boundingRect() const;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        void setWidth(qreal width);

    private:
        const SlotView& m_slotView;
        qreal m_width {};
        QPen m_pen;
};
