#include "MoveNewState.hpp"

#include "Process/ScenarioModel.hpp"

Scenario::Command::MoveNewState::MoveNewState(ObjectPath &&scenarioPath,
                                              id_type<EventModel> eventId,
                                              const double y):
    SerializableCommand {"ScenarioControl",
             commandName(),
             description()},
    m_path(std::move(scenarioPath)),
    m_eventId{eventId},
    m_y{y}
{

}

void Scenario::Command::MoveNewState::undo()
{

}

void Scenario::Command::MoveNewState::redo()
{
//    auto scenar = m_path.find<ScenarioModel>();
//    scenar.event(m_eventId).setHeightPercentage(m_y);
}

void Scenario::Command::MoveNewState::serializeImpl(QDataStream & s) const
{
    s << m_path << m_eventId << m_y;
}

void Scenario::Command::MoveNewState::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_eventId >> m_y;
}
