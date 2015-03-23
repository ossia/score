#include "AbstractTimeRulerView.hpp"

#include <QPainter>

#include <QDebug>
#include <QGraphicsScene>

AbstractTimeRulerView::AbstractTimeRulerView() :
    m_width{800},
    m_graduationHeight{-10}
{
    setY(-35);
}

QRectF AbstractTimeRulerView::boundingRect() const
{
    return QRectF{0, 0, m_width * 2, -m_height};
}

void AbstractTimeRulerView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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

void AbstractTimeRulerView::setHeight(qreal newHeight)
{
    prepareGeometryChange();
    m_height = newHeight;
}

void AbstractTimeRulerView::setWidth(qreal newWidth)
{
    prepareGeometryChange();
    m_width = newWidth;
}

void AbstractTimeRulerView::setGraduationsStyle(double size, int delta, QString format, int mark)
{
    prepareGeometryChange();
    m_graduationsSpacing = size;
    m_graduationDelta = delta;
    m_timeFormat = format;
    m_intervalsBeetwenMark = mark;
}

void AbstractTimeRulerView::setFormat(QString format)
{
    m_timeFormat = format;
}


