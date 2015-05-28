#pragma once
#include <QGraphicsItem>

class ConditionView : public QGraphicsItem
{
    public:
        enum class State {
            Disabled, Waiting, True, False
        };

        ConditionView(QGraphicsItem* parent);

        using QGraphicsItem::QGraphicsItem;
        QRectF boundingRect() const;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget);
    private:
        State m_currentState{State::Waiting};
        QPainterPath m_Cpath, m_trianglePath;
};
