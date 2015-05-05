#include "LocalTimeRulerView.hpp"

LocalTimeRulerView::LocalTimeRulerView():
    AbstractTimeRulerView()
{
    m_graduationHeight = 10;
    m_textPosition = 1.75 * m_graduationHeight;
    m_height = 3 * m_graduationHeight;
    m_color = QColor{155,0,70};
    m_timeFormat = "ss''''z";
    setZValue(1);
}

LocalTimeRulerView::~LocalTimeRulerView()
{

}

QRectF LocalTimeRulerView::boundingRect() const
{
    return QRectF{0, 0, m_width * 2, m_height};
}
