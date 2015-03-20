#pragma once
#include <QGraphicsObject>

class QCustomPlot;
class QMouseEvent;

class MyPoint;
class QCustomPlotCurve : public QGraphicsObject
{
        Q_OBJECT
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

        void setCurrentPointPos(QPointF);
        void removeFakePoint();
        void pointMoved(QPointF);

    signals:
        void pointMovingFinished(double oldx, double newx, double newy);

    private:
        void on_mouseMoveEvent(QMouseEvent*);

        QPointF pointUnderMouse(QMouseEvent*);

        QCustomPlot* m_plot{};
        MyPoint* m_fakePoint{};

        QSizeF m_size;
};
