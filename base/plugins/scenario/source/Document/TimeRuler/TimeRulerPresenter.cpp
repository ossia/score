#include "TimeRulerPresenter.hpp"

#include "TimeRulerView.hpp"

TimeRulerPresenter::TimeRulerPresenter(TimeRulerView* view, QObject *parent) :
    m_view{view}
{
    m_startPoint.addMSecs(0);
    m_duration.addMSecs(0);
}

TimeRulerPresenter::~TimeRulerPresenter()
{

}

void TimeRulerPresenter::setDuration(TimeValue dur)
{
    if (m_duration != dur)
    {
        m_duration = dur;
        m_view->setWidth(m_duration.msec() * m_pixelPerMillis);
    }
}

void TimeRulerPresenter::setStartPoint(TimeValue dur)
{
    if (m_startPoint != dur)
    {
        m_startPoint = dur;
        m_view->setX(m_startPoint.msec() * m_pixelPerMillis);
    }
}

void TimeRulerPresenter::setPixelPerMillis(double factor)
{
    if (factor != m_pixelPerMillis)
    {
        m_pixelPerMillis = factor;
        m_view->setGraduationsSpacing(factor * 10000); // 10 sec for now
        m_view->setWidth(m_duration.msec() * m_pixelPerMillis);
        m_view->setX(m_startPoint.msec() * m_pixelPerMillis);
    }
}

