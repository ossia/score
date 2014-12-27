#include "EventModelSerialization.hpp"
#include "Document/Event/EventModel.hpp"

template<> void Visitor<Reader<DataStream>>::visit<EventModel>(EventModel& ev)
{
	/* TODO
	m_stream << static_cast<const IdentifiedObject&>(ev);

	m_stream << ev.previousConstraints()
			 << ev.nextConstraints()
			 << ev.heightPercentage()
			 << ev.constraintsYPos()
			 << ev.bottomY()
			 << ev.topY();

	m_stream << ev.date(); // should be in OSSIA API

	auto states = ev.states();
	m_stream << int(states.size());
	for(auto& state : states)
	{
		m_stream << *state;
	}
	m_stream << ev;
	*/
}

template<> void Visitor<Writer<DataStream>>::visit<EventModel>(EventModel& ev)
{
	/* TODO
	int m_stream;

	QVector<int> prevCstr, nextCstr;
	QMap<int, double> cstrYPos;
	double heightPercentage, bottomY, topY;
	int date;
	m_stream >> prevCstr
			>> nextCstr
			>> heightPercentage
			>> cstrYPos
			>> bottomY
			>> topY;

	m_stream >> date; // should be in OSSIA API

	ev.setPreviousConstraints(std::move(prevCstr));
	ev.setNextConstraints(std::move(nextCstr));
	ev.setHeightPercentage(heightPercentage);
	ev.setConstraintsYPos(cstrYPos);
	ev.setBottomY(bottomY);
	ev.setTopY(topY);
	ev.setDate(date);


	int numStates{};
	m_stream >> numStates;
	for(; numStates --> 0;)
	{
		ev.createState(m_stream);
	}

	ev.setOSSIATimeNode(new OSSIA::TimeNode);
	// TODO load the timenode
	*/
}
