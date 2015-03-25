#include "TimeRulerPresenter.hpp"

#include "TimeRulerView.hpp"

TimeRulerPresenter::TimeRulerPresenter(TimeRulerView* view, QObject *parent) :
    AbstractTimeRuler{view, parent}
{
    m_startPoint.addMSecs(0);
    m_duration.addMSecs(0);
}

TimeRulerPresenter::~TimeRulerPresenter()
{

}

void TimeRulerPresenter::setStartPoint(TimeValue dur)
{
    if (m_startPoint != dur)
    {
        m_startPoint = dur;
    }
}
