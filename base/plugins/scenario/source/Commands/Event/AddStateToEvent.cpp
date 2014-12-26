#include "AddStateToEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"

#include <QDebug>

using namespace iscore;
using namespace Scenario::Command;

AddStateToEvent::AddStateToEvent():
	SerializableCommand{"ScenarioControl",
						"AddStateToEventCommand",
						QObject::tr("State and message creation")}
{

}

AddStateToEvent::AddStateToEvent(ObjectPath&& eventPath, QString message):
	SerializableCommand{"ScenarioControl",
						"AddStateToEventCommand",
						QObject::tr("State and message creation")},
	m_path{std::move(eventPath)},
	m_message(message)
{
	auto event = static_cast<EventModel*>(m_path.find());
	m_stateId = getNextId(event->states());
}

void AddStateToEvent::undo()
{
	auto event = static_cast<EventModel*>(m_path.find());

	event->removeState(m_stateId);
}

void AddStateToEvent::redo()
{
	auto event = static_cast<EventModel*>(m_path.find());
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

void AddStateToEvent::serializeImpl(QDataStream&)
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}

void AddStateToEvent::deserializeImpl(QDataStream&)
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}
