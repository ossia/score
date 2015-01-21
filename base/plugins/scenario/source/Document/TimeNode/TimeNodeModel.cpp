#include "TimeNodeModel.hpp"

TimeNodeModel::TimeNodeModel(id_type<TimeNodeModel> id, QObject *parent):
	IdentifiedObjectAlternative<TimeNodeModel>{id, "TimeNodeModel", parent}
{

}

TimeNodeModel::TimeNodeModel(id_type<TimeNodeModel> id, int date, QObject *parent):
	TimeNodeModel{id, parent}
{
	m_date = date;
}

TimeNodeModel::~TimeNodeModel()
{

}

void TimeNodeModel::addEvent(id_type<EventModel> eventId)
{
	m_events.push_back(eventId);
}

bool TimeNodeModel::removeEvent(id_type<EventModel> eventId)
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

void TimeNodeModel::setDate(int date)
{
	m_date = date;
}
double TimeNodeModel::y() const
{
	return m_y;
}

void TimeNodeModel::setY(double y)
{
	m_y = y;
}


