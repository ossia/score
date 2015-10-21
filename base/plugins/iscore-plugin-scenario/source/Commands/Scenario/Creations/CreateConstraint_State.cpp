#include "CreateConstraint_State.hpp"

#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/ScenarioModel.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include "Process/Algorithms/VerticalMovePolicy.hpp"

using namespace Scenario::Command;
CreateConstraint_State::CreateConstraint_State(
        const ScenarioModel& scenario,
        const Id<StateModel>& startState,
        const Id<EventModel>& endEvent,
        double endStateY):
    iscore::SerializableCommand{factoryName(), commandName(), description()},
    m_newState{iscore::id_generator::getStrongId(scenario.states)},
    m_command{scenario,
              startState,
              m_newState},
    m_endEvent{endEvent},
    m_stateY{endStateY}
{
}

CreateConstraint_State::CreateConstraint_State(
        const Path<ScenarioModel>& scenario,
        const Id<StateModel> &startState,
        const Id<EventModel> &endEvent,
        double endStateY):
    CreateConstraint_State{scenario.find(),
                           startState,
                           endEvent,
                           endStateY}
{

}


void CreateConstraint_State::undo() const
{
    m_command.undo();

    ScenarioCreate<StateModel>::undo(
                m_newState,
                m_command.scenarioPath().find());
    updateEventExtent(m_endEvent, m_command.scenarioPath().find());
}

void CreateConstraint_State::redo() const
{
    auto& scenar = m_command.scenarioPath().find();

    // Create the end state
    ScenarioCreate<StateModel>::redo(
                m_newState,
                scenar.events.at(m_endEvent),
                m_stateY,
                scenar);

    // The constraint between
    m_command.redo();
    updateEventExtent(m_endEvent, scenar);
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
