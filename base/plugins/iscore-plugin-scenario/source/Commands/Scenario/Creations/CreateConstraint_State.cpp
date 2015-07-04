#include "CreateConstraint_State.hpp"

#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/ScenarioModel.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace Scenario::Command;
CreateConstraint_State::CreateConstraint_State(
        ScenarioModel& scenario,
        const id_type<DisplayedStateModel>& startState,
        const id_type<EventModel>& endEvent,
        double endStateY):
    iscore::SerializableCommand{"ScenarioControl", commandName(), description()},
    m_endStateId{getStrongId(scenario.displayedStates())},
    m_createConstraint{iscore::IDocument::path(scenario),
                       startState,
                       m_endStateId},
    m_endEvent{endEvent},
    m_endStateY{endStateY}
{

}


void CreateConstraint_State::undo()
{
    m_createConstraint.undo();

    ScenarioCreate<DisplayedStateModel>::undo(
                m_endStateId,
                m_createConstraint.scenarioPath().find<ScenarioModel>());
}

void CreateConstraint_State::redo()
{
    auto& scenar = m_createConstraint.scenarioPath().find<ScenarioModel>();

    // Create the end state
    ScenarioCreate<DisplayedStateModel>::redo(
                m_endStateId,
                scenar.event(m_endEvent),
                m_endStateY,
                scenar);

    // The constraint between
    m_createConstraint.redo();
}

void CreateConstraint_State::serializeImpl(QDataStream& s) const
{
    s << m_endStateId << m_createConstraint.serialize() << m_endEvent << m_endStateY;
}

void CreateConstraint_State::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> m_endStateId >> b >> m_endEvent >> m_endStateY;

    m_createConstraint.deserialize(b);
}
