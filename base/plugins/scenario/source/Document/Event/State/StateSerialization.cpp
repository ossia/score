#include <interface/serialization/DataStreamVisitor.hpp>
#include "State.hpp"

template<>
void Visitor<Reader<DataStream> >::readFrom(const State& state)
{
	readFrom(static_cast<const IdentifiedObject&>(state));

	m_stream << state.messages();
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
}