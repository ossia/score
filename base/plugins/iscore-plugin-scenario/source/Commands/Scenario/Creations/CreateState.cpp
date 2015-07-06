#include "CreateState.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/Algorithms/VerticalMovePolicy.hpp"

#include "Process/ScenarioModel.hpp"
using namespace Scenario::Command;
CreateState::CreateState(const ScenarioModel &scenario, const id_type<EventModel> &event, double stateY) :
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_path {iscore::IDocument::path(scenario)},
    m_newState{getStrongId(scenario.states())},
    m_event{event},
    m_stateY{stateY}
{

}

CreateState::CreateState(
        const ObjectPath &scenarioPath,
        const id_type<EventModel> &event,
        double stateY):
    CreateState{scenarioPath.find<ScenarioModel>(), event, stateY}
{

}

void CreateState::undo()
{
    auto& scenar = m_path.find<ScenarioModel>();

    ScenarioCreate<StateModel>::undo(
                m_newState,
                scenar);
    updateEventExtent(m_event, scenar);
}

void CreateState::redo()
{
    auto& scenar = m_path.find<ScenarioModel>();

    // Create the end state
    ScenarioCreate<StateModel>::redo(
                m_newState,
                scenar.event(m_event),
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


