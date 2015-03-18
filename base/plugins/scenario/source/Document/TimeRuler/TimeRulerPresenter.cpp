#include "TimeRulerPresenter.hpp"

#include "TimeRulerView.hpp"

TimeRulerPresenter::TimeRulerPresenter(TimeRulerView* view, QObject *parent) :
    m_view{view}
{

}

TimeRulerPresenter::~TimeRulerPresenter()
{

}

void TimeRulerPresenter::setDuration(TimeValue dur)
{
    m_view->setDuration(dur);
}

