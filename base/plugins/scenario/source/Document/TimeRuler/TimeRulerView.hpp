#pragma once

#include <QGraphicsObject>
#include "ProcessInterface/TimeValue.hpp"


class TimeRulerView : public QGraphicsObject
{
    public:
        TimeRulerView();
        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        void setHeight(qreal newHeight);
        void setWidth(qreal newWidth);

        void setPixelPerMillis(double newFactor);
        void setDuration(TimeValue dur);

    signals:

    public slots:
        void updateGraduationsSize();

    private:
        qreal m_height {35};
        qreal m_width {800};

        TimeValue m_duration;

        double m_pixelPerMillis {0.03};
//        double m_graduationSize {};
//        double m_subGraduationSize {};
};
