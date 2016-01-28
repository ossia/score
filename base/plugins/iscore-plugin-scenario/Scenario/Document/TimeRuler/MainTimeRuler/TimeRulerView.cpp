#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>

#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>
#include "TimeRulerView.hpp"

namespace Scenario
{
TimeRulerView::TimeRulerView() :
    AbstractTimeRulerView{}
{
    m_height = -3 * m_graduationHeight;
    m_textPosition = 1.15 * m_graduationHeight;
    m_color = ScenarioStyle::instance().TimeRuler;
}

QRectF TimeRulerView::boundingRect() const
{
    return QRectF{0, -m_height, m_width * 2, m_height};
}
}
