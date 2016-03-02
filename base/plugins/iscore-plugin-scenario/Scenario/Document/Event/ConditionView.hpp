#pragma once

#include <QGraphicsItem>
#include <QPainterPath>
#include <QRect>
#include <Process/Style/ColorReference.hpp>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{

class ConditionView final : public QGraphicsItem
{
    public:

        ConditionView(ColorRef color, QGraphicsItem* parent);

        using QGraphicsItem::QGraphicsItem;
        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;
        void changeHeight(qreal newH);

        void setColor(ColorRef c)
        { m_color = c;  update();}

    private:

        ColorRef m_color;
        QPainterPath m_trianglePath;
        QPainterPath m_Cpath;
        qreal m_height{0};
        qreal m_width{40};
        qreal m_CHeight{27};
};

}
