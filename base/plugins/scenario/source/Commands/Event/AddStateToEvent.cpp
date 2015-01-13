#include "AddStateToEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"

#include <QDebug>

using namespace iscore;
using namespace Scenario::Command;

AddStateToEvent::AddStateToEvent():
	SerializableCommand{"ScenarioControl",
						"AddStateToEvent",
						QObject::tr("State and message creation")}
{

}

AddStateToEvent::AddStateToEvent(ObjectPath&& eventPath, QString message):
	SerializableCommand{"ScenarioControl",
						"AddStateToEvent",
						QObject::tr("State and message creation")},
	m_path{std::move(eventPath)},
	m_message(message)
{
	auto event = m_path.find<EventModel>();
	m_stateId = getNextId(event->states());
}

void AddStateToEvent::undo()
{
	auto event = m_path.find<EventModel>();

	event->removeState(m_stateId);
}

void AddStateToEvent::redo()
{
	auto event = m_path.find<EventModel>();
	FakeState* state = new FakeState{m_stateId, event};
	state->addMessage(m_message);

	event->addState(state);
}

int AddStateToEvent::id() const
{
	return 1;
}

bool AddStateToEvent::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddStateToEvent::serializeImpl(QDataStream& s)
{
	s << m_path << m_message << m_stateId;
}

void AddStateToEvent::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_message >> m_stateId;
}
