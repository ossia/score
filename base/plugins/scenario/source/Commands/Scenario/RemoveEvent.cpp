#include "RemoveEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

#include <core/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveEvent::RemoveEvent():
	SerializableCommand{"ScenarioControl",
                        "RemoveEvent",
						QObject::tr("Remove event and pre-constraints")}
{
}


RemoveEvent::RemoveEvent(ObjectPath&& eventPath):
	SerializableCommand{"ScenarioControl",
                        "RemoveEvent",
						QObject::tr("Remove event and pre-constraints")},
	m_path{std::move(eventPath)}
{

	auto event = m_path.find<EventModel>();
	for(const State* state: event->states())
	{
		QByteArray arr;
		Serializer<DataStream> s{&arr};
		s.readFrom(*state);
		m_serializedStates.push_back(arr);
	}
}

void RemoveEvent::undo()
{
	auto event = m_path.find<EventModel>();
	for(auto& serializedState : m_serializedStates)
	{
		Deserializer<DataStream> s{&serializedState};
		event->addState(new FakeState{s, event});
	}
}

void RemoveEvent::redo()
{
	auto event = m_path.find<EventModel>();
    auto scenar = m_path.find<ScenarioProcessSharedModel>();

    scenar->removeEvent(event->id());

}

int RemoveEvent::id() const
{
	return 1;
}

bool RemoveEvent::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveEvent::serializeImpl(QDataStream& s)
{
	s << m_path << m_serializedStates;
}

void RemoveEvent::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_serializedStates;
}
