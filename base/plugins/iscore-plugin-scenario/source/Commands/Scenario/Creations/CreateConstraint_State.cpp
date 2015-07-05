#include "CreateConstraint_State.hpp"

#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/ScenarioModel.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace Scenario::Command;
CreateConstraint_State::CreateConstraint_State(
        const ScenarioModel& scenario,
        const id_type<StateModel>& startState,
        const id_type<EventModel>& endEvent,
        double endStateY):
    iscore::SerializableCommand{"ScenarioControl", commandName(), description()},
    m_newState{getStrongId(scenario.states())},
    m_command{iscore::IDocument::path(scenario),
                       startState,
                       m_newState},
    m_endEvent{endEvent},
    m_stateY{endStateY}
{

}


void CreateConstraint_State::undo()
{
    m_command.undo();

    ScenarioCreate<StateModel>::undo(
                m_newState,
                m_command.scenarioPath().find<ScenarioModel>());
}

void CreateConstraint_State::redo()
{
    auto& scenar = m_command.scenarioPath().find<ScenarioModel>();

    // Create the end state
    ScenarioCreate<StateModel>::redo(
                m_newState,
                scenar.event(m_endEvent),
                m_stateY,
                scenar);

    // The constraint between
    m_command.redo();
}

void CreateConstraint_State::serializeImpl(QDataStream& s) const
{
    s << m_newState << m_command.serialize() << m_endEvent << m_stateY;
}

void CreateConstraint_State::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> m_newState >> b >> m_endEvent >> m_stateY;

    m_command.deserialize(b);
}
