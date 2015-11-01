#include "LocalTimeRulerPresenter.hpp"

LocalTimeRulerPresenter::LocalTimeRulerPresenter(LocalTimeRulerView *view, QObject *parent) :
    AbstractTimeRuler{view, parent}
{

}

LocalTimeRulerPresenter::LocalTimeRulerPresenter(LocalTimeRulerView *view,
                                                 TimeValue startDate,
                                                 TimeValue duration,
                                                 double pixPerMillis,
                                                 QObject *parent) :
    LocalTimeRulerPresenter{view, parent}
{
    m_startPoint = startDate;
    m_pixelPerMillis = pixPerMillis;
}

LocalTimeRulerPresenter::~LocalTimeRulerPresenter()
{

}

void LocalTimeRulerPresenter::setStartPoint(TimeValue dur)
{
    if (m_startPoint != dur)
    {
        m_startPoint = dur;
        m_view->setX((m_startPoint).msec() * m_pixelPerMillis + m_totalScroll);
    }
}

