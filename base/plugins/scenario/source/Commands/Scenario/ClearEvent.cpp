#include "ClearEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

#include <core/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

ClearEvent::ClearEvent():
	ClearEvent{{}}
{
}


ClearEvent::ClearEvent(ObjectPath&& eventPath):
	iscore::SerializableCommand{"ScenarioControl",
								"EmptyEventCommand",
								QObject::tr("Remove event and pre-constraints")},
	m_path{std::move(eventPath)}
{

	auto event = static_cast<EventModel*>(m_path.find());
	for(const State* state: event->states())
	{
		QByteArray arr;
		Serializer<DataStream> s{&arr};
		s.readFrom(*state);
		m_serializedStates.push_back(arr);
	}
}

void ClearEvent::undo()
{
	auto event = static_cast<EventModel*>(m_path.find());
	for(auto& serializedState : m_serializedStates)
	{
		Deserializer<DataStream> s{&serializedState};
		event->addState(new FakeState{s, event});
	}
}

void ClearEvent::redo()
{
	auto event = static_cast<EventModel*>(m_path.find());
	for(auto& state : event->states())
	{
		event->removeState((SettableIdentifier::identifier_type)state->id());
	}

}

int ClearEvent::id() const
{
	return 1;
}

bool ClearEvent::mergeWith(const QUndoCommand* other)
{
	return false;
}

void ClearEvent::serializeImpl(QDataStream& s)
{
	s << m_path << m_serializedStates;
}

void ClearEvent::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_serializedStates;
}
