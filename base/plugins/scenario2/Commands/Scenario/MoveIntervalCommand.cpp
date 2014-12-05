#include "MoveIntervalCommand.hpp"
#include <Document/Process/ScenarioProcessSharedModel.hpp>
#include <Document/Interval/IntervalModel.hpp>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/view/View.hpp>


using namespace iscore;


MoveIntervalCommand::MoveIntervalCommand(ObjectPath &&scenarioPath, int intervalId, int endEvent, double heightPosition):
    SerializableCommand{"ScenarioControl",
                        "MoveEventCommand",
                        QObject::tr("Constraint move")},
    m_scenarioPath(std::move(scenarioPath)),
    m_intervalId{intervalId},
    m_endEventId{endEvent},
    m_heightPosition{heightPosition}
{

}

void MoveIntervalCommand::undo()
{
    auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
    if(scenar != nullptr)
    {
        m_heightPosition = scenar->interval(m_intervalId)->heightPercentage();
        scenar->moveEventAndInterval(m_endEventId, m_oldHeightPosition);
        scenar->moveInterval(m_intervalId, m_oldHeightPosition);
    }
}

void MoveIntervalCommand::redo()
{
    auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
    if(scenar != nullptr)
    {
        m_oldHeightPosition = scenar->interval(m_intervalId)->heightPercentage();
        scenar->moveInterval(m_intervalId, m_heightPosition);
        scenar->moveEventAndInterval(m_endEventId, m_heightPosition);
        qDebug() << "doing command " << m_oldHeightPosition << " -> " << m_heightPosition;
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
    s << m_scenarioPath << m_intervalId << m_endEventId << m_heightPosition;
}

void MoveIntervalCommand::deserializeImpl(QDataStream& s)
{
    s >> m_scenarioPath >> m_intervalId >> m_endEventId >> m_heightPosition;
}
