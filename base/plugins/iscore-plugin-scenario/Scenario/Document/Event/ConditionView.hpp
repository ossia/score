#pragma once

#include <QGraphicsItem>
#include <QPainterPath>
#include <QRect>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class ConditionView final : public QGraphicsItem
{
    public:
        enum class State {
            Disabled, Waiting, True, False
        };

        ConditionView(QGraphicsItem* parent);

        using QGraphicsItem::QGraphicsItem;
        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;
        void changeHeight(qreal newH);
    private:
        State m_currentState{State::Waiting};
        QPainterPath m_trianglePath;
        QPainterPath m_Cpath;
        qreal m_height{0};
        qreal m_width{40};
        qreal m_CHeight{27};
};
