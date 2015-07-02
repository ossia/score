#include "MoveNewEvent.hpp"

#include "Process/ScenarioModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

MoveNewEvent::MoveNewEvent():
    SerializableCommand {"ScenarioControl",
             commandName(),
             description()},
    m_cmd{new MoveEvent}
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
    m_cmd{new MoveEvent(std::move(scenarioPath), eventId, date, ExpandMode::Fixed)},
    m_y{y},
    m_yLocked{yLocked}
{

}

MoveNewEvent::~MoveNewEvent()
{
    delete m_cmd;
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
     s << m_path << m_cmd->serialize() << m_constraintId << m_y << m_yLocked;
}

void MoveNewEvent::deserializeImpl(QDataStream & s)
{
    QByteArray a;
    s >> m_path >> a >> m_constraintId >> m_y >> m_yLocked;

    m_cmd->deserialize(a);
}

