#pragma once
#include "CurveItem.hpp"


class Curve;

class CurvePoint : public CurveItem
{
    public:
        CurvePoint(Curve* parent);

        QPointF val;

        QRectF boundingRect() const override;

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        void lockVertically()
        {
            m_locked = true;
        }


    protected:
        QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
        void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    private:
        Curve* m_curve{};
        bool m_hover{false};
        bool m_locked{false};
};
