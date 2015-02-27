#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>


class TimeNodeView : public QGraphicsObject
{
        Q_OBJECT

    public:
        TimeNodeView(QGraphicsObject* parent);
        ~TimeNodeView() override = default ;

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        // QGraphicsItem interface
        QRectF boundingRect() const override;

        void setExtremities(int top, int bottom);
        void mousePressEvent(QGraphicsSceneMouseEvent* m) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* m) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* m) override;

        void setMoving(bool);

    public slots:
        void changeColor(QColor);

    signals:
        void timeNodeMoved(QPointF);
        void timeNodeReleased();
        void timeNodeSelectionChanged(bool);

    protected:
        virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    private:
        int m_top {0};
        int m_bottom {0};

        QPointF m_clickedPoint {};
        QColor m_color;
        bool m_moving {false};
};
