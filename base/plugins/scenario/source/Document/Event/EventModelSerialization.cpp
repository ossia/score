#include <interface/serialization/DataStreamVisitor.hpp>
#include <interface/serialization/JSONVisitor.hpp>
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/Event/State/State.hpp"

#include <API/Headers/Editor/TimeNode.h>




template<> void Visitor<Reader<DataStream>>::readFrom(const EventModel& ev)
{
	readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));

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
	QVector<id_type<ConstraintModel>> prevCstr, nextCstr;
	QMap<id_type<ConstraintModel>, double> cstrYPos;
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




template<> void Visitor<Reader<JSON>>::readFrom(const EventModel& ev)
{
	readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));

	m_obj["PreviousConstraints"] = toJsonArray(ev.previousConstraints());
	m_obj["NextConstraints"] = toJsonArray(ev.nextConstraints());
	m_obj["HeightPercentage"] = ev.heightPercentage();
	m_obj["ConstraintsYPos"] = toJsonMap(ev.constraintsYPos());
	m_obj["BottomY"] = ev.bottomY();
	m_obj["TopY"] = ev.topY();
	m_obj["Date"] = ev.date(); // should be in OSSIA API

	QJsonArray arr;
	for(auto state : ev.states())
	{
		arr.push_back(toJsonObject(*state));
	}
	m_obj["States"] = arr;

	// TODO save OSSIA::TimeNode
}

template<> void Visitor<Writer<JSON>>::writeTo(EventModel& ev)
{
	QVector<id_type<ConstraintModel>> prevCstr, nextCstr;
	;
	fromJsonArray(m_obj["PreviousConstraints"].toArray(), prevCstr);
	fromJsonArray(m_obj["NextConstraints"].toArray(), nextCstr);
	auto ymap = fromJsonMap<id_type<ConstraintModel>>(m_obj["ConstraintsYPos"].toArray());
	ev.setPreviousConstraints(std::move(prevCstr));
	ev.setNextConstraints(std::move(nextCstr));
	ev.setConstraintsYPos(std::move(ymap));

	ev.setHeightPercentage(m_obj["HeightPercentage"].toDouble());
	ev.setBottomY(m_obj["BottomY"].toInt());
	ev.setTopY(m_obj["TopY"].toInt());
	ev.setDate(m_obj["Date"].toInt());

	QJsonArray states = m_obj["States"].toArray();
	for(auto json_vref : states)
	{
		Deserializer<JSON> deserializer{json_vref.toObject()};
		FakeState* state = new FakeState{deserializer, &ev};
		ev.addState(state);
	}

	ev.setOSSIATimeNode(new OSSIA::TimeNode);
}
