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
        void setGraduationsSpacing(double size);
        void setDownGraduations();

    private:
        qreal m_height {};
        qreal m_width {};

        qreal m_graduationsSpacing {};
        qreal m_textPosition{};
        int m_graduationHeight {};

        QColor m_color;
};
