#include "Process/TimeValue.hpp"
#include "Scenario/Document/TimeRuler/AbstractTimeRuler.hpp"
#include "TimeRulerPresenter.hpp"
#include "TimeRulerView.hpp"

class QObject;

TimeRulerPresenter::TimeRulerPresenter(TimeRulerView* view, QObject *parent) :
    AbstractTimeRuler{view, parent}
{
    m_startPoint.addMSecs(0);
}

TimeRulerPresenter::~TimeRulerPresenter()
{

}

TimeRulerView* TimeRulerPresenter::view()
{
    return static_cast<TimeRulerView*>(m_view);
}
