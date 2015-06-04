#include "TimeRulerView.hpp"

#include <QApplication>
#include <QPalette>

TimeRulerView::TimeRulerView() :
    AbstractTimeRulerView{}
{
    m_height = -3 * m_graduationHeight;
    m_textPosition = 1.05 * m_graduationHeight;
    m_color = qApp->palette("ScenarioPalette").alternateBase().color();
}

QRectF TimeRulerView::boundingRect() const
{
    return QRectF{0, -m_height, m_width * 2, m_height};
}


