#pragma once
#include <QGraphicsItem>
#include <QPen>
class SlotView;
class SlotHandle final : public QGraphicsItem
{
    public:
        SlotHandle(const SlotView& slotView,
                   QGraphicsItem* parent);

        int type() const override
        { return QGraphicsItem::UserType + 5; }

        static constexpr double handleHeight()
        {
            return 3.;
        }

        const SlotView& slotView() const
        {
            return m_slotView;
        }

        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

        void setWidth(qreal width);

    private:
        const SlotView& m_slotView;
        qreal m_width {};
        QPen m_pen;
};
