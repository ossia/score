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
    m_heightPosition{data.relativeY},
    m_deltaX{data.x}
{

}

void MoveEventCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());

	m_heightPosition = scenar->event(m_eventId)->heightPercentage();
    scenar->moveEventAndConstraint(m_eventId, - m_deltaX, m_oldHeightPosition);
}

void MoveEventCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
	m_oldHeightPosition = scenar->event(m_eventId)->heightPercentage();

    scenar->moveEventAndConstraint(m_eventId, m_deltaX, m_heightPosition);
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
    s << m_scenarioPath << m_eventId << m_heightPosition << m_deltaX;
}

void MoveEventCommand::deserializeImpl(QDataStream& s)
{
    s >> m_scenarioPath >> m_eventId >> m_heightPosition >> m_deltaX;
}
