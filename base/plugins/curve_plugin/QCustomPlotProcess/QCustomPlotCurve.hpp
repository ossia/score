#pragma once
#include <QGraphicsObject>

class QCustomPlot;
class QMouseEvent;

class MyPoint;
class QCustomPlotCurve : public QGraphicsObject
{
        Q_OBJECT
        friend class MyPoint;
    public:
        QCustomPlotCurve(QGraphicsItem* parent);

        void setPoints(QList<QPointF> list);
        void setSize(const QSizeF& size);

        QRectF boundingRect() const
        {
            return {0, 0, m_size.width(), m_size.height()};
        }

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
        {
        }

    signals:
        void pointMovingFinished(double oldx, double newx, double newy);
        void pointCreated(QPointF pt);

    private:
        QPointF pointUnderMouse(QMouseEvent*);
        void on_mouseMoveEvent(QMouseEvent*);
        void on_mousePressEvent(QMouseEvent*);

        // From MyPoint
        void setCurrentPointPos(QPointF);
        void removeFakePoint();
        void pointMoved(QPointF);


        QCustomPlot* m_plot{};
        MyPoint* m_fakePoint{};

        QSizeF m_size;
        QPointF m_backedUpPoint{-1, -1};
};
