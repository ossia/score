#include "CreateEventAfterEventCommand.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventData.hpp"

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <QApplication>

using namespace iscore;



CreateEventAfterEventCommand::CreateEventAfterEventCommand():
	SerializableCommand{"ScenarioControl",
						"CreateEventAfterEventCommand",
						QObject::tr("Event creation")}
{
}

CreateEventAfterEventCommand::CreateEventAfterEventCommand(ObjectPath &&scenarioPath, EventData data):
    SerializableCommand{"ScenarioControl",
                        "CreateEventAfterEventCommand",
                        QObject::tr("Event creation")},
    m_path(std::move(scenarioPath)),
    m_firstEventId{data.id},
    m_time{data.x},
    m_heightPosition{data.relativeY}
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
		auto ids = scenar->createIntervalAndEndEventFromEvent(m_firstEventId, m_time, m_heightPosition);
		m_intervalId = std::get<0>(ids);
	}
}

int CreateEventAfterEventCommand::id() const
{
	return 1;
}

bool CreateEventAfterEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void CreateEventAfterEventCommand::serializeImpl(QDataStream& s)
{
	s << m_path << m_firstEventId << m_time;
}

void CreateEventAfterEventCommand::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_firstEventId >> m_time;
}
