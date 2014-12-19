#include "AddStateToEventCommand.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"

#include <QDebug>

using namespace iscore;


AddStateToEventCommand::AddStateToEventCommand():
	SerializableCommand{"ScenarioControl",
						"AddStateToEventCommand",
						QObject::tr("State and message creation")}
{

}

AddStateToEventCommand::AddStateToEventCommand(ObjectPath&& eventPath, QString message):
	SerializableCommand{"ScenarioControl",
						"AddStateToEventCommand",
						QObject::tr("State and message creation")},
	m_path(std::move(eventPath)),
	m_message(message)
{
	auto event = static_cast<EventModel*>(m_path.find());
	m_stateId = getNextId(event->states());
}

void AddStateToEventCommand::undo()
{
	auto event = static_cast<EventModel*>(m_path.find());

	event->removeState(m_stateId);
}

void AddStateToEventCommand::redo()
{
	auto event = static_cast<EventModel*>(m_path.find());
	FakeState* state = new FakeState{m_stateId, event};
	state->addMessage(m_message);

	event->addState(state);
}

int AddStateToEventCommand::id() const
{
	return 1;
}

bool AddStateToEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}


// TODO
void AddStateToEventCommand::serializeImpl(QDataStream&)
{
}

void AddStateToEventCommand::deserializeImpl(QDataStream&)
{
}
