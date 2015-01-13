#include "TimeNodeModel.hpp"

TimeNodeModel::TimeNodeModel(int id, QObject *parent):
    IdentifiedObject{id, "TimeNodeModel", parent}
{

}

TimeNodeModel::TimeNodeModel(int id, int date, QObject *parent):
    TimeNodeModel(id, parent)
{
    m_date = date;
}

TimeNodeModel::~TimeNodeModel()
{

}

void TimeNodeModel::addEvent(int eventId)
{
    m_events.push_back(eventId);
}

bool TimeNodeModel::removeEvent(int eventId)
{
    if(m_events.indexOf(eventId) >= 0)
    {
        m_events.remove(m_events.indexOf(eventId));
        return true;
    }
    return false;
}

double TimeNodeModel::top() const
{
    return m_topY;
}

double TimeNodeModel::bottom() const
{
    return m_bottomY;
}

int TimeNodeModel::date() const
{
    return m_date;
}

void TimeNodeModel::setTop(double top)
{
    m_topY = top;
}

void TimeNodeModel::setBottom(double bottom)
{
    m_bottomY = bottom;
}

