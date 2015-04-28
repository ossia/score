#include "TimeRulerView.hpp"

#include <QPainter>

#include <QGraphicsScene>

TimeRulerView::TimeRulerView() :
    AbstractTimeRulerView{}
{
    m_height = -3 * m_graduationHeight;
    m_textPosition = 1.05 * m_graduationHeight;
    m_color = QColor(50, 0, 155);
}

QRectF TimeRulerView::boundingRect() const
{
    return QRectF{0, -m_height, m_width * 2, m_height};
}


