#include "MoveNewEvent.hpp"

#include "Process/ScenarioModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

MoveNewEvent::MoveNewEvent(ObjectPath&& scenarioPath,
               id_type<ConstraintModel> constraintId,
               id_type<EventModel> eventId,
               const TimeValue& date,
               const double y,
               bool yLocked) :
    SerializableCommand {"ScenarioControl",
             commandName(),
             description()},
    m_path {std::move(scenarioPath)},
    m_constraintId {constraintId},
    m_cmd{new MoveEvent(std::move(scenarioPath), eventId, date, ExpandMode::Fixed)},
    m_y{y},
    m_yLocked{yLocked}
{

}

void MoveNewEvent::undo()
{
    m_cmd->undo();
}

void MoveNewEvent::redo()
{
    m_cmd->redo();
    if(! m_yLocked)
    {
        auto& scenar = m_cmd->path().find<ScenarioModel>();
        scenar.constraint(m_constraintId).setHeightPercentage(m_y);
    }
}

void MoveNewEvent::serializeImpl(QDataStream & s) const
{
     s << m_cmd->serialize() << m_constraintId << m_y;
}

void MoveNewEvent::deserializeImpl(QDataStream & s)
{
    QByteArray a;
    s >> a >> m_constraintId >> m_y;

    m_cmd->deserialize(a);
}

