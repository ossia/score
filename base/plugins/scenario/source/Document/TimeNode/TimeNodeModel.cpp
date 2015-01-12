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

void TimeNodeModel::removeEvent(int eventId)
{
    if(m_events.indexOf(eventId) >= 0)
    {
        m_events.remove(m_events.indexOf(eventId));
    }
}

