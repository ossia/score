#include "MoveNewEvent.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Algorithms/VerticalMovePolicy.hpp"


using namespace iscore;
using namespace Scenario::Command;

MoveNewEvent::MoveNewEvent():
    SerializableCommand {"ScenarioControl",
             commandName(),
             description()}
{

}

MoveNewEvent::MoveNewEvent(ObjectPath&& scenarioPath,
               id_type<ConstraintModel> constraintId,
               id_type<EventModel> eventId,
               const TimeValue& date,
               const double y,
               bool yLocked) :
    SerializableCommand {"ScenarioControl",
             commandName(),
             description()},
    m_path {scenarioPath},
    m_constraintId {constraintId},
    m_cmd{std::move(scenarioPath), eventId, date, ExpandMode::Fixed},
    m_y{y},
    m_yLocked{yLocked}
{

}

void MoveNewEvent::undo()
{
    m_cmd.undo();
    // TODO we don't undo the contraint pos ?
}

void MoveNewEvent::redo()
{
    m_cmd.redo();
    if(! m_yLocked)
    {
        updateConstraintVerticalPos(
                    m_y,
                    m_constraintId,
                    m_cmd.path().find<ScenarioModel>());
    }
}

void MoveNewEvent::serializeImpl(QDataStream & s) const
{
     s << m_path << m_cmd.serialize() << m_constraintId << m_y << m_yLocked;
}

void MoveNewEvent::deserializeImpl(QDataStream & s)
{
    QByteArray a;
    s >> m_path >> a >> m_constraintId >> m_y >> m_yLocked;

    m_cmd.deserialize(a);
}
