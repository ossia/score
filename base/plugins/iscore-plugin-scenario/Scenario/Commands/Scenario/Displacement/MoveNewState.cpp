#include "MoveNewState.hpp"
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

Scenario::Command::MoveNewState::MoveNewState(
        Path<ScenarioModel>&& scenarioPath,
        const Id<StateModel>& stateId,
        const double y):
    m_path(std::move(scenarioPath)),
    m_stateId{stateId},
    m_y{y}
{
    auto& scenar = m_path.find();
    m_oldy = scenar.state(m_stateId).heightPercentage();
}

void Scenario::Command::MoveNewState::undo() const
{
    auto& scenar = m_path.find();
    auto& state = scenar.state(m_stateId);
    state.setHeightPercentage(m_oldy);

    updateEventExtent(state.eventId(), scenar);
}

void Scenario::Command::MoveNewState::redo() const
{
    auto& scenar = m_path.find();
    auto& state = scenar.state(m_stateId);
    state.setHeightPercentage(m_y);

    updateEventExtent(state.eventId(), scenar);
}

void Scenario::Command::MoveNewState::serializeImpl(QDataStream & s) const
{
    s << m_path << m_stateId << m_oldy << m_y;
}

void Scenario::Command::MoveNewState::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_stateId >> m_oldy >> m_y;
}
