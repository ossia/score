#pragma once
#include <QGraphicsItem>
class CurveSegmentModel;
class CurveSegmentView : public QGraphicsObject
{
        Q_OBJECT
    public:
        CurveSegmentView(CurveSegmentModel* model, QGraphicsItem* parent);
        int type() const override;
        QRectF boundingRect() const override;
        QPainterPath shape() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        CurveSegmentModel& model() const
        { return *m_model; }
        void setRect(const QRectF& theRect);

        void setSelected(bool selected);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        void updatePoints();
        // Takes a table of points and draws them in a square given by the boundingRect
        // QGraphicsItem interface
        QVector<QPointF> m_points; // each between rect.topLeft() :: rect.bottomRight()
        QRectF m_rect;

        CurveSegmentModel* m_model{};
        bool m_selected{};

        QPainterPath m_shape;
};
