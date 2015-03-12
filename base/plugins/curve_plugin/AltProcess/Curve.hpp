#pragma once

#include "CurvePoint.hpp"
#include "CurveSegment.hpp"

class Curve : public QGraphicsObject
{
        Q_OBJECT
    public:
        Curve(QList<QPointF> points, QGraphicsItem* parent);

        virtual ~Curve() = default;

        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;


        void updatePoint(CurvePoint* pt, double newX, double newY);

        double width() const
        { return m_width; }
        double height() const
        { return m_height; }

        void setWidth(double d);
        void setHeight(double d);

    signals:
        void pointMovingFinished(double oldx, double newx, double newy);
        void pointCreated(QPointF newpt);
        void mousePressed();

    public slots:
        void setSize(QSize s);
        void editingBegins(CurvePoint* pt);
        void editingFinished(CurvePoint* pt);

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* evt) override;
        void mousePressEvent(QGraphicsSceneMouseEvent* evt) override;

    private:
        CurvePoint* makePoint(double x, double y);
        CurveSegment* makeSegment(CurvePoint* pt1, CurvePoint* pt2);
        void addPoint(CurvePoint* pt);
        void addSegment(CurveSegment* segmt);

        double m_width{};
        double m_height{};

        // List is sorted : point / segment / point / segment / point
        QList<CurveItem*> m_curveObjects;

        double m_currentX;
};
