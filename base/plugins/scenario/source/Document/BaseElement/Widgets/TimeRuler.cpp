#include "TimeRuler.hpp"

#include "ProcessInterface/TimeValue.hpp"
#include <QPainter>

#include <QDebug>
#include <QGraphicsScene>

TimeRuler::TimeRuler()
{
//    m_duration.addSecs(30);
//    m_width = m_duration.msec() * m_pixelPerMillis;
}

QRectF TimeRuler::boundingRect() const
{
    return QRectF{0, -25, m_width, 25};
}

void TimeRuler::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(QPen(QBrush(QColor(100, 0, 255)), m_height, Qt::SolidLine));
//    painter->drawLine(-m_width / 2, 0, m_width / 2, 0);

    double pixPerSec = m_pixelPerMillis * 1000;
    double t = 0;
    int i = 0;

    while (t < m_width)
    {
        painter->drawLine(t, 0, t, -10);
        painter->drawText(t, -20, QString::number(10 * i));
        t += 10 * pixPerSec;
        i++;
        qDebug () << i ;
    }
    qDebug() << m_width << scene()->width() << pixPerSec;
}

void TimeRuler::setHeight(qreal newHeight)
{
    prepareGeometryChange();
    m_height = newHeight;
}

void TimeRuler::setWidth(qreal newWidth)
{
    prepareGeometryChange();
    m_width = newWidth;
}

void TimeRuler::setPixelPerMillis(double newFactor)
{
    m_pixelPerMillis = newFactor;
//    update();
}

void TimeRuler::updateGraduationsSize()
{
    return;
}


