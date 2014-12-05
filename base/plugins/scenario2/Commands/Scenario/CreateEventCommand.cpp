#include "CreateEventCommand.hpp"
#include <Document/Process/ScenarioProcessSharedModel.hpp>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/view/View.hpp>
using namespace iscore;

// TODO maybe the to-be-added event id should be created here ?
CreateEventCommand::CreateEventCommand():
	SerializableCommand{"ScenarioControl",
						"CreateEventCommand",
						QObject::tr("Event creation")}
{

}

CreateEventCommand::CreateEventCommand(ObjectPath&& scenarioPath, int time, float heightPosition):
	SerializableCommand{"ScenarioControl",
						"CreateEventCommand",
						QObject::tr("Event creation")},
	m_path(std::move(scenarioPath)),
    m_time{time},
    m_heightPosition{heightPosition}
{

}

#include <QDebug>
void CreateEventCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());
	if(scenar != nullptr)
	{
		scenar->undo_createIntervalAndEndEventFromStartEvent(m_intervalId);
		m_intervalId = -1;
	}
}

void CreateEventCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());
	if(scenar != nullptr)
	{
        auto ids = scenar->createIntervalAndEndEventFromStartEvent(m_time, m_heightPosition);
		m_intervalId = std::get<0>(ids);
	}
}

int CreateEventCommand::id() const
{
	return 1;
}

bool CreateEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void CreateEventCommand::serializeImpl(QDataStream& s)
{
    s << m_path << m_time << m_heightPosition;
}

void CreateEventCommand::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_time >> m_heightPosition;
}
