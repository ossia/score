#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <algorithm>

#include "MoveNewState.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

#include <iscore/tools/SettableIdentifier.hpp>

Scenario::Command::MoveNewState::MoveNewState(
        Path<Scenario::ScenarioModel>&& scenarioPath,
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

void Scenario::Command::MoveNewState::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_stateId << m_oldy << m_y;
}

void Scenario::Command::MoveNewState::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_stateId >> m_oldy >> m_y;
}
