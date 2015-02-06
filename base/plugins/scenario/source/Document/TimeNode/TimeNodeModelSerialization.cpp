#include "TimeNodeModel.hpp"


template<>
void Visitor<Reader<DataStream> >::readFrom(const TimeNodeModel& timenode)
{
	readFrom(static_cast<const IdentifiedObject<TimeNodeModel>&>(timenode));

	m_stream << timenode.m_topY
			 << timenode.m_bottomY
			 << timenode.m_date
			 << timenode.m_y
			 << timenode.m_events;

	insertDelimiter();
}

template<>
void Visitor<Writer<DataStream> >::writeTo(TimeNodeModel& timenode)
{
	m_stream >> timenode.m_topY
			 >> timenode.m_bottomY
			 >> timenode.m_date
			 >> timenode.m_y
			 >> timenode.m_events;

	checkDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const TimeNodeModel& timenode)
{
	readFrom(static_cast<const IdentifiedObject<TimeNodeModel>&>(timenode));

	m_obj["Top"] = timenode.top();
	m_obj["Bottom"] = timenode.bottom();
	m_obj["Date"] = timenode.date();
	m_obj["Y"] = timenode.y();
	m_obj["Events"] = toJsonArray(timenode.m_events);
}

template<>
void Visitor<Writer<JSON>>::writeTo(TimeNodeModel& timenode)
{
	timenode.m_topY = m_obj["Top"].toDouble();
	timenode.m_bottomY = m_obj["Bottom"].toDouble();
	timenode.m_date = m_obj["Date"].toInt();
	timenode.m_y = m_obj["Y"].toDouble();

	fromJsonArray(m_obj["Events"].toArray(), timenode.m_events);
}
