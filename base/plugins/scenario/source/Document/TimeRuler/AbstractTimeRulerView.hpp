#pragma once

#include <QGraphicsObject>
#include "ProcessInterface/TimeValue.hpp"


class AbstractTimeRulerView : public QGraphicsObject
{
    public:
        AbstractTimeRulerView();
        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        void setHeight(qreal newHeight);
        void setWidth(qreal newWidth);

    signals:

    public slots:
        void setGraduationsStyle(double size, int delta, QString format, int mark);
        void setFormat(QString);

    protected:
        qreal m_height {};
        qreal m_width {};

        qreal m_graduationsSpacing {};
        int m_graduationDelta {};
        QString m_timeFormat{};
        int m_intervalsBeetwenMark {};

        qreal m_textPosition{};
        int m_graduationHeight {};

        QColor m_color;
};
