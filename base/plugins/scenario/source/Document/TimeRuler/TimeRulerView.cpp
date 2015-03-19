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

    double t = 0;
    int i = 0;

    while (t < m_width + 10)
    {
        painter->setPen(QPen(QBrush(m_color), 2, Qt::SolidLine));
        painter->drawLine(t, 0, t, m_graduationHeight);
        painter->drawText(t, m_textPosition, QString::number(10 * i));

        // GRID
//        painter->setPen(QPen(QBrush(QColor(100, 0, 255)), 1, Qt::DashDotDotLine));
//        painter->drawLine(t, 0, t, 500);

        t += m_graduationsSpacing;
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

void TimeRulerView::setGraduationsSpacing(double size)
{
    prepareGeometryChange();
    m_graduationsSpacing = size;
}

void TimeRulerView::setDownGraduations()
{
    prepareGeometryChange();
    m_graduationHeight = 10;
    m_textPosition = 3 * m_graduationHeight;
    m_height = - m_height;
    m_color = QColor{155,0,70};
    setZValue(1);
}


