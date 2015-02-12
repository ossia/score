#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>


class TimeNodeView : public QGraphicsObject
{
    Q_OBJECT

    public:
        TimeNodeView(QGraphicsObject* parent);
        virtual ~TimeNodeView() = default;

        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);

        // QGraphicsItem interface
        QRectF boundingRect() const;

        void setExtremities(int top, int bottom);
        void mousePressEvent(QGraphicsSceneMouseEvent* m);
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* m);

    signals:
        void timeNodeReleased(QPointF);
        void timeNodeSelected();

    private:
        int m_top{0};
        int m_bottom{0};

        QPointF m_clickedPoint{};
};
