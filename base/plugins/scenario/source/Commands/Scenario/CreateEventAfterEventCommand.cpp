#include "CreateEventAfterEventCommand.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
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
    m_firstEventId{data.eventClickedId},
    m_time{data.x},
    m_heightPosition{data.relativeY}
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	m_createdEventId = getNextId(scenar->events());
	m_createdConstraintId = getNextId(scenar->constraints());
}

void CreateEventAfterEventCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());
	if(scenar != nullptr)
	{
		scenar->undo_createConstraintAndEndEventFromEvent(m_createdConstraintId);
	}
}

void CreateEventAfterEventCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());
	if(scenar != nullptr)
	{
		scenar->createConstraintAndEndEventFromEvent(m_firstEventId,
												   m_time,
												   m_heightPosition,
												   m_createdConstraintId,
												   m_createdEventId);
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
