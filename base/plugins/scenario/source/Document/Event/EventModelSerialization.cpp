#include <interface/serialization/DataStreamVisitor.hpp>
#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"
#include "Document/Event/State/StateSerialization.hpp"

#include <API/Headers/Editor/TimeNode.h>


template<> void Visitor<Reader<DataStream>>::readFrom(const EventModel& ev)
{
	readFrom(static_cast<const IdentifiedObject&>(ev));

	m_stream << ev.previousConstraints()
			 << ev.nextConstraints()
			 << ev.heightPercentage()
			 << ev.constraintsYPos()
			 << ev.bottomY()
			 << ev.topY();

	m_stream << ev.date(); // should be in OSSIA API

	auto states = ev.states();
	m_stream << int(states.size());
	for(auto state : states)
	{
		readFrom(*state);
	}

	// TODO save OSSIA::TimeNode
}

template<> void Visitor<Writer<DataStream>>::writeTo(EventModel& ev)
{
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
	ev.setConstraintsYPos(std::move(cstrYPos));
	ev.setBottomY(bottomY);
	ev.setTopY(topY);
	ev.setDate(date);


	int numStates{};
	m_stream >> numStates;
	for(; numStates --> 0;)
	{
		FakeState* state = new FakeState{*this, &ev};
		ev.addState(state);
	}

	ev.setOSSIATimeNode(new OSSIA::TimeNode);
	// TODO load the timenode
}
