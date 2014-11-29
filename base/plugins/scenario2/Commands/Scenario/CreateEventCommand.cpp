#include "CreateEventCommand.hpp"
#include <Document/Process/ScenarioProcessSharedModel.hpp>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/view/View.hpp>
using namespace iscore;

// TODO maybe the to-be-added event id should be created here ?
CreatEventCommand::CreatEventCommand(ObjectPath&& scenarioPath, int time):
	SerializableCommand{"ScenarioControl",
						"CreateEventCommand",
						QObject::tr("Event creation")},
	m_path(std::move(scenarioPath)),
	m_time{time}
{

}

#include <QDebug>
void CreatEventCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());
	if(scenar != nullptr)
	{
		scenar->undo_createIntervalAndEndEventFromStartEvent(m_intervalId);
		m_intervalId = -1;
	}
}

void CreatEventCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());
	if(scenar != nullptr)
	{
		auto ids = scenar->createIntervalAndEndEventFromStartEvent(m_time);
		m_intervalId = std::get<0>(ids);
	}
}

int CreatEventCommand::id() const
{
	return 1;
}

bool CreatEventCommand::mergeWith(const QUndoCommand* other)
{
}

void CreatEventCommand::serializeImpl(QDataStream& s)
{
}

void CreatEventCommand::deserializeImpl(QDataStream& s)
{
}
