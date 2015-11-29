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
    private:
        State m_currentState{State::Waiting};
        QPainterPath m_Cpath, m_trianglePath;
};
