#include "CreateIntervalCommand.hpp"

#include <Document/Process/ScenarioProcessSharedModel.hpp>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/view/View.hpp>
using namespace iscore;


CreateEventAfterEventCommand::CreateEventAfterEventCommand(ObjectPath&& scenarioPath,
														   int firstEventId,
														   int time):
	SerializableCommand{"ScenarioControl",
						"CreateEventAfterEventCommand",
						QObject::tr("Event creation")},
	m_path(std::move(scenarioPath)),
	m_firstEventId{firstEventId},
	m_time{time}
{

}

void CreateEventAfterEventCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());
	if(scenar != nullptr)
	{
		scenar->undo_createIntervalAndEndEventFromEvent(m_intervalId);
		m_intervalId = -1;
	}
}

void CreateEventAfterEventCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());
	if(scenar != nullptr)
	{
		auto ids = scenar->createIntervalAndEndEventFromEvent(m_firstEventId, m_time);
		m_intervalId = std::get<0>(ids);
	}
}

int CreateEventAfterEventCommand::id() const
{
	return 1;
}

bool CreateEventAfterEventCommand::mergeWith(const QUndoCommand* other)
{
}

void CreateEventAfterEventCommand::serializeImpl(QDataStream&)
{
}

void CreateEventAfterEventCommand::deserializeImpl(QDataStream&)
{
}
