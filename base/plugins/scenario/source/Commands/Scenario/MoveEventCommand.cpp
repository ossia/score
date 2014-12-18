#include "MoveEventCommand.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <QApplication>


using namespace iscore;


MoveEventCommand::MoveEventCommand(ObjectPath &&scenarioPath, EventData data):
	SerializableCommand{"ScenarioControl",
						"MoveEventCommand",
						QObject::tr("Event move")},
	m_scenarioPath(std::move(scenarioPath)),
	m_eventId{data.eventClickedId},
	m_newHeightPosition{data.relativeY},
	m_newX{data.x}
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
	auto ev = scenar->event(m_eventId);
	m_oldHeightPosition = ev->heightPercentage();
	m_oldX = ev->date();
}

void MoveEventCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());

	scenar->moveEventAndConstraint(m_eventId, m_oldX, m_oldHeightPosition);
}

void MoveEventCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());

	scenar->moveEventAndConstraint(m_eventId, m_newX, m_newHeightPosition);
}

int MoveEventCommand::id() const
{
	return 1;
}

bool MoveEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void MoveEventCommand::serializeImpl(QDataStream& s)
{
	s << m_scenarioPath << m_eventId
	  << m_oldHeightPosition << m_newHeightPosition << m_oldX << m_newX;
}

void MoveEventCommand::deserializeImpl(QDataStream& s)
{
	s >> m_scenarioPath >> m_eventId
	  >> m_oldHeightPosition >> m_newHeightPosition >> m_oldX >> m_newX;
}
