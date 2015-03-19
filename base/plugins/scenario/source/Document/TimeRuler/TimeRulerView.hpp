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

    signals:

    public slots:
        void setGraduationsSize(double size);

    private:
        qreal m_height {35};
        qreal m_width {800};

        double m_graduationSize {};
//        double m_subGraduationSize {};
};
