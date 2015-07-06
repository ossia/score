#include "CreateConstraint_State_Event.hpp"

#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/ScenarioModel.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace Scenario::Command;
CreateConstraint_State_Event::CreateConstraint_State_Event(
        const ScenarioModel &scenario,
        const id_type<StateModel>& startState,
        const id_type<TimeNodeModel>& endTimeNode,
        double endStateY):
    iscore::SerializableCommand{"ScenarioControl", commandName(), description()},
    m_newEvent{getStrongId(scenario.events())},
    m_command{scenario,
              startState,
              m_newEvent,
              endStateY},
    m_endTimeNode{endTimeNode}
{

}

CreateConstraint_State_Event::CreateConstraint_State_Event(
        const ObjectPath &scenarioPath,
        const id_type<StateModel> &startState,
        const id_type<TimeNodeModel> &endTimeNode,
        double endStateY):
    CreateConstraint_State_Event{
        scenarioPath.find<ScenarioModel>(),
        startState,
        endTimeNode,
        endStateY}
{

}

void CreateConstraint_State_Event::undo()
{
    m_command.undo();

    ScenarioCreate<EventModel>::undo(
                m_newEvent,
                m_command.scenarioPath().find<ScenarioModel>());
}

void CreateConstraint_State_Event::redo()
{
    auto& scenar = m_command.scenarioPath().find<ScenarioModel>();

    // Create the end event
    ScenarioCreate<EventModel>::redo(
                m_newEvent,
                scenar.timeNode(m_endTimeNode),
                {{m_command.endStateY(), m_command.endStateY()}},
                scenar);

    // The state + constraint between
    m_command.redo();
}

void CreateConstraint_State_Event::serializeImpl(QDataStream& s) const
{
    s << m_newEvent << m_command.serialize() << m_endTimeNode;
}

void CreateConstraint_State_Event::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> m_newEvent >> b >> m_endTimeNode;

    m_command.deserialize(b);
}
