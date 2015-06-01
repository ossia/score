#pragma once
#include <QGraphicsItem>

class CurveView : public QGraphicsItem
{
    public:
    using QGraphicsItem::QGraphicsItem;
        void setRect(const QRectF& theRect);

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

        void updateSubitems();

    signals:

    private:
        QRectF rect; // The rect in which the whole curve must fit.
};

