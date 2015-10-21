#include "AbstractTimeRulerView.hpp"

#include <cmath>
#include <QPainter>
#include <QGraphicsScene>
#include <Document/TimeRuler/AbstractTimeRuler.hpp>


AbstractTimeRulerView::AbstractTimeRulerView() :
    m_width{800},
    m_graduationsSpacing{10},
    m_graduationDelta{10},
    m_intervalsBeetwenMark{1},
    m_graduationHeight{-10}
{
    setY(-25);
}

void AbstractTimeRulerView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    static const QFont TimeRulerFont("APCCourier-Bold", 8);

    const auto brush = QBrush(m_color);
    painter->setPen(QPen(brush, 2, Qt::SolidLine));
    painter->drawLine(0, 0, m_width, 0);

    painter->setPen(QPen(brush, 1, Qt::SolidLine));
    painter->drawPath(m_path);

    painter->setFont(TimeRulerFont);

    if (m_width > 0)
    {
        for (const auto& mark : m_marks)
        {
            painter->drawText(m_marks.key(mark) + 6 , m_textPosition, mark.toString(m_timeFormat));
        }
    }
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
    createRulerPath();
}

void AbstractTimeRulerView::setGraduationsStyle(double size, int delta, QString format, int mark)
{
    prepareGeometryChange();
    m_graduationsSpacing = size;
    m_graduationDelta = delta;
    m_timeFormat = format;
    m_intervalsBeetwenMark = mark;
    createRulerPath();
}

void AbstractTimeRulerView::setFormat(QString format)
{
    m_timeFormat = format;
}

void AbstractTimeRulerView::createRulerPath()
{
    m_marks.clear();

    QPainterPath path;

    if(m_width == 0)
    {
        m_path = path;
        return;
    }

    // If we are between two graduations, we adjust our origin.
    auto big_delta = m_graduationDelta * 5;
    auto prev_big_grad_msec = std::floor(m_pres->startPoint().msec() / big_delta) * big_delta;

    auto startTime = TimeValue::fromMsecs(m_pres->startPoint().msec() - prev_big_grad_msec);
    QTime time = TimeValue::fromMsecs(prev_big_grad_msec).toQTime();
    double t = -startTime.toPixels(1./m_pres->pixelsPerMillis());

    int i = 0;
    double gradSize;

    while (t < m_width + 1)
    {
        /*
        gradSize = 0.5;
        if (m_intervalsBeetwenMark % 2 == 0)
        {
            if (i % (m_intervalsBeetwenMark / 2) == 0)
            {
                gradSize = 1;
            }
        }
        */
        if (i % m_intervalsBeetwenMark == 0)
        {
            m_marks[t] = time;
            gradSize = 3;
            path.addRect(t, 0, 1, m_graduationHeight * gradSize);
        }

        t += m_graduationsSpacing;
        time = time.addMSecs(m_graduationDelta);
        i++;
    }
    m_path = path;
    update();
}


