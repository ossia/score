#include "TimeRulerView.hpp"

#include <QPainter>

#include <QDebug>
#include <QGraphicsScene>

TimeRulerView::TimeRulerView() :
    m_width{800},
    m_graduationHeight{-10}
{
    setY(-35);

    m_height = -3 * m_graduationHeight;
    m_textPosition = 1.5 * m_graduationHeight;
    m_color = QColor(50, 0, 155);
}

QRectF TimeRulerView::boundingRect() const
{
    if (m_height > 0)
        return QRectF{0, -m_height, m_width * 2, m_height};

    return QRectF{0, 0, m_width * 2, -m_height};
}

void TimeRulerView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(QPen(QBrush(m_color), 2, Qt::SolidLine));
    painter->drawLine(0, 0, m_width, 0);

    QTime time{0,0,0,0};

    double t = 0;
    int i = 0;
    double gradSize;

    while (t < m_width + 10)
    {
        painter->setPen(QPen(QBrush(m_color), 1, Qt::SolidLine));
        gradSize = 0.5;

        if (m_intervalsBeetwenMark % 2 == 0)
        {
            if (i % (m_intervalsBeetwenMark / 2) == 0)
            {
                gradSize = 1;
            }
        }

        if (i % m_intervalsBeetwenMark == 0)
        {
            painter->drawText(t, m_textPosition, time.toString(m_timeFormat));
            gradSize = 1.5;
        }
        painter->drawLine(t, 0, t, m_graduationHeight * gradSize);

        t += m_graduationsSpacing;
        time = time.addMSecs(m_graduationDelta);
        i++;
    }
    painter->drawLine(m_width, 0, m_width, m_graduationHeight);

}

void TimeRulerView::setHeight(qreal newHeight)
{
    prepareGeometryChange();
    m_height = newHeight;
}

void TimeRulerView::setWidth(qreal newWidth)
{
    prepareGeometryChange();
    m_width = newWidth;
}

void TimeRulerView::setGraduationsStyle(double size, int delta, QString format, int mark)
{
    prepareGeometryChange();
    m_graduationsSpacing = size;
    m_graduationDelta = delta;
    m_timeFormat = format;
    m_intervalsBeetwenMark = mark;
}

void TimeRulerView::setDownGraduations()
{
    prepareGeometryChange();
    m_graduationHeight = 10;
    m_textPosition = 3 * m_graduationHeight;
    m_height = - m_height;
    m_color = QColor{155,0,70};
    m_timeFormat = "ss''''z";
    setZValue(1);
}

void TimeRulerView::setFormat(QString format)
{
    m_timeFormat = format;
}


