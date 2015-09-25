#pragma once
#include <QGraphicsItem>

class TriggerView : public QGraphicsItem
{
    public:
        TriggerView(QGraphicsItem* parent);
        QRectF boundingRect() const;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        int type() const override
        { return QGraphicsItem::UserType + 7; }
};
