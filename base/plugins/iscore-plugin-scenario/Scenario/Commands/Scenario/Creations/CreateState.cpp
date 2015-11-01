#include "CreateState.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
using namespace Scenario::Command;
CreateState::CreateState(const ScenarioModel &scenario, const Id<EventModel> &event, double stateY) :
    SerializableCommand{factoryName(),
                        commandName(),
                        description()},
    m_path {scenario},
    m_newState{getStrongId(scenario.states)},
    m_event{event},
    m_stateY{stateY}
{

}

CreateState::CreateState(
        const Path<ScenarioModel> &scenarioPath,
        const Id<EventModel> &event,
        double stateY):
    CreateState{scenarioPath.find(), event, stateY}
{

}

void CreateState::undo() const
{
    auto& scenar = m_path.find();

    ScenarioCreate<StateModel>::undo(
                m_newState,
                scenar);
    updateEventExtent(m_event, scenar);
}

void CreateState::redo() const
{
    auto& scenar = m_path.find();

    // Create the end state
    ScenarioCreate<StateModel>::redo(
                m_newState,
                scenar.events.at(m_event),
                m_stateY,
                scenar);

    updateEventExtent(m_event, scenar);
}


void CreateState::serializeImpl(QDataStream & s) const
{
    s << m_path << m_newState << m_event << m_stateY;
}


void CreateState::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_newState >> m_event >> m_stateY;
}


