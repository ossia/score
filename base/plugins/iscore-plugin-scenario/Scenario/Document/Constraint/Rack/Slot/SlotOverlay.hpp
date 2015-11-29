#pragma once
#include <qglobal.h>
#include <qgraphicsitem.h>
#include <qrect.h>

class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class SlotView;

class SlotOverlay final : public QGraphicsItem
{
    public:
        SlotOverlay(SlotView* parent);

        static constexpr int static_type()
        { return QGraphicsItem::UserType + 8; }
        int type() const override
        { return static_type(); }

        const SlotView& slotView() const
        { return m_slotView; }

        QRectF boundingRect() const override;

        void setHeight(qreal height);
        void setWidth(qreal height);

        virtual void paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget) override;

        void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        const SlotView&m_slotView;
};
