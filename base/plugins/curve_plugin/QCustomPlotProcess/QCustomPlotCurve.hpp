#pragma once
#include <QGraphicsObject>
#include "qcustomplot/qcustomplot.h"

class QCustomPlot;
class QMouseEvent;
class PointsLayer;
class MyPoint;
class QCustomPlotCurve : public QGraphicsObject
{
        Q_OBJECT
        friend class MyPoint;
    public:
        QCustomPlotCurve(QGraphicsItem* parent);

        void enable();

        void disable();

        void setPoints(const QList<QPointF>& list);
        void setSize(const QSizeF& size);


        QList<QPointF> pointsToPixels(const QCPDataMap& data);
        QRectF boundingRect() const;

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

        void redraw();

    signals:
        void pointMovingFinished(double oldx, double newx, double newy);
        void pointCreated(QPointF pt);

        void mousePressed();

    private:
        QPointF pointUnderMouse(QMouseEvent*);
        void on_mouseMoveEvent(QMouseEvent*);
        void on_mousePressEvent(QMouseEvent*);

        // From MyPoint
        void setCurrentPointPos(QPointF);
        void removeFakePoint();
        void pointMoved(QPointF);


        QCustomPlot* m_plot{};
        PointsLayer* m_points{};
        MyPoint* m_fakePoint{};

        QSizeF m_size;
        QPointF m_backedUpPoint{-1, -1};

        QPen m_pen;
};
