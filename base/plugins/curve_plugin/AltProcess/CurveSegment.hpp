#pragma once
#include "CurveItem.hpp"
class CurvePoint;
class Curve;
class CurveSegment : public CurveItem
{
    public:
        CurveSegment(Curve* parent);
        CurvePoint* origin{};
        CurvePoint* dest{};

        QRectF boundingRect() const override;

        bool hovering() const;

    protected:
        void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

    private:
        bool m_hover{false};
};

class LinearCurveSegment : public CurveSegment
{
    public:
        using CurveSegment::CurveSegment;

       // QPainterPath shape() const override;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};
