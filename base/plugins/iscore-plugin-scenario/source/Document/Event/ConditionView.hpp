#pragma once
#include <QGraphicsItem>

class ConditionView : public QGraphicsItem
{
    public:
        using QGraphicsItem::QGraphicsItem;
        QRectF boundingRect() const;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget);
};
