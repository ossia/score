#include "AddStateToEventCommand.hpp"
#include <Document/Event/EventModel.hpp>
#include <Document/Event/State/State.hpp>
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

}

void AddStateToEventCommand::undo()
{
	auto event = static_cast<EventModel*>(m_path.find());
	if(event != nullptr)
	{
		event->removeMessage(m_message);
	}
}

void AddStateToEventCommand::redo()
{
	auto event = static_cast<EventModel*>(m_path.find());
	if(event != nullptr)
	{
		event->addMessage(m_message);
	}
}

int AddStateToEventCommand::id() const
{
	return 1;
}

bool AddStateToEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddStateToEventCommand::serializeImpl(QDataStream&)
{
}

void AddStateToEventCommand::deserializeImpl(QDataStream&)
{
}
