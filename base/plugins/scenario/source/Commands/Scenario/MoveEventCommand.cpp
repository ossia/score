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
    m_time{data.x},
    m_heightPosition{data.relativeY}
{

}

void MoveEventCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
	if(scenar != nullptr)
	{
		m_time = scenar->event(m_eventId)->date();
		m_heightPosition = scenar->event(m_eventId)->heightPercentage();
		scenar->moveEventAndConstraint(m_eventId, m_oldTime, m_oldHeightPosition);
	}
}

void MoveEventCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
	if(scenar != nullptr)
	{
		m_oldTime = scenar->event(m_eventId)->date();
		m_oldHeightPosition = scenar->event(m_eventId)->heightPercentage();
		scenar->moveEventAndConstraint(m_eventId, m_time, m_heightPosition);
	}
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
	s << m_scenarioPath << m_eventId << m_time << m_heightPosition;
}

void MoveEventCommand::deserializeImpl(QDataStream& s)
{
	s >> m_scenarioPath >> m_eventId >> m_time >> m_heightPosition;
}
