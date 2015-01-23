#include <interface/serialization/DataStreamVisitor.hpp>
#include <interface/serialization/JSONVisitor.hpp>
#include "State.hpp"

template<>
void Visitor<Reader<DataStream> >::readFrom(const State& state)
{
	readFrom(static_cast<const IdentifiedObject<State>&>(state));

	m_stream << state.messages();
	insertDelimiter();
}

template<>
void Visitor<Writer<DataStream> >::writeTo(State& state)
{
	QStringList messages;
	m_stream >> messages;

	for(auto& message : messages)
	{
		state.addMessage(message);
	}
	checkDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const State& state)
{
	readFrom(static_cast<const IdentifiedObject<State>&>(state));

	m_obj["Messages"] = QJsonArray::fromStringList(state.messages());
}

template<>
void Visitor<Writer<JSON>>::writeTo(State& state)
{
	for(auto& message : m_obj["Messages"].toArray().toVariantList())
	{
		state.addMessage(message.toString());
	}
}
