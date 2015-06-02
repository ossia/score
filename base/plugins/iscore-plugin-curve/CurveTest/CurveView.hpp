#pragma once
#include <QGraphicsItem>

class CurveView : public QGraphicsObject
{
        Q_OBJECT
    public:
        using QGraphicsObject::QGraphicsObject;
        void setRect(const QRectF& theRect);

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void escPressed();

    private:
        QRectF m_rect; // The rect in which the whole curve must fit.
};

