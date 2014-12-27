#include "StateSerialization.hpp"
#include "State.hpp"

template<>
void Visitor<Reader<DataStream> >::visit(const State& state)
{
	visit(static_cast<const IdentifiedObject&>(state));

	m_stream << state.messages();
}

template<>
void Visitor<Writer<DataStream> >::visit(State& state)
{
	QStringList messages;
	m_stream >> messages;

	for(auto& message : messages)
	{
		state.addMessage(message);
	}
}