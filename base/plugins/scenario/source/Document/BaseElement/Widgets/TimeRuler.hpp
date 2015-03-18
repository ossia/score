#pragma once

#include <QGraphicsObject>

//class TimeValue;

class TimeRuler : public QGraphicsObject
{
    public:
        TimeRuler();
        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        void setHeight(qreal newHeight);
        void setWidth(qreal newWidth);

        void setPixelPerMillis(double newFactor);

    signals:

    public slots:
        void updateGraduationsSize();

    private:
        qreal m_height {2};
        qreal m_width {800};

//        TimeValue m_duration {};

        double m_pixelPerMillis {0.02};
        double m_graduationSize {};
        double m_subGraduationSize {};
};
