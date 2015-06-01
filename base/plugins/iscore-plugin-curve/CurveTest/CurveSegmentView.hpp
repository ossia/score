#pragma once
#include <QGraphicsItem>
class CurveSegmentModel;
class CurveSegmentView : public QGraphicsItem
{
        // Takes a table of points and draws them in a square given by the boundingRect
        // QGraphicsItem interface
        QVector<QPointF> points; // each between rect.topLeft() :: rect.bottomRight()
        QRectF rect;

    public:
        CurveSegmentView(CurveSegmentModel* model, QGraphicsItem* parent);

        void setRect(const QRectF& theRect);
        void setPoints(QVector<QPointF>&& thePoints);

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};
