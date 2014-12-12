#include "MoveIntervalCommand.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Interval/IntervalModel.hpp"

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <QApplication>

using namespace iscore;

// @todo : maybe should we use deplacement value and not absolute ending point.

MoveIntervalCommand::MoveIntervalCommand(ObjectPath &&scenarioPath, int intervalId, int endEvent, double deltaHeight):
	SerializableCommand{"ScenarioControl",
						"MoveEventCommand",
						QObject::tr("Constraint move")},
	m_scenarioPath(std::move(scenarioPath)),
	m_intervalId{intervalId},
	m_endEventId{endEvent},
    m_deltaHeight{deltaHeight}
{

}

void MoveIntervalCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
	if(scenar != nullptr)
	{
        scenar->moveInterval(m_intervalId, m_oldHeightPosition);
	}
}

void MoveIntervalCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
	if(scenar != nullptr)
	{
		m_oldHeightPosition = scenar->interval(m_intervalId)->heightPercentage();
//        m_heightPosition = m_oldHeightPosition + m_deltaHeight;
        scenar->moveInterval(m_intervalId, m_oldHeightPosition + m_deltaHeight);
	}
}

int MoveIntervalCommand::id() const
{
	return 1;
}

bool MoveIntervalCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void MoveIntervalCommand::serializeImpl(QDataStream& s)
{
    s << m_scenarioPath << m_intervalId << m_endEventId << m_deltaHeight;
}

void MoveIntervalCommand::deserializeImpl(QDataStream& s)
{
    s >> m_scenarioPath >> m_intervalId >> m_endEventId >> m_deltaHeight;
}
