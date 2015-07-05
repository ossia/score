#include "CreateConstraint_State_Event_TimeNode.hpp"

#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/ScenarioModel.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace Scenario::Command;
CreateConstraint_State_Event_TimeNode::CreateConstraint_State_Event_TimeNode(
        const ScenarioModel& scenario,
        const id_type<StateModel>& startState,
        const TimeValue& date,
        double endStateY):
    iscore::SerializableCommand{"ScenarioControl", commandName(), description()},
    m_newTimeNode{getStrongId(scenario.timeNodes())},
    m_command{scenario,
              startState,
              m_newTimeNode,
              endStateY},
    m_date{date}
{

}


void CreateConstraint_State_Event_TimeNode::undo()
{
    m_command.undo();

    ScenarioCreate<TimeNodeModel>::undo(
                m_newTimeNode,
                m_command.scenarioPath().find<ScenarioModel>());
}

void CreateConstraint_State_Event_TimeNode::redo()
{
    auto& scenar = m_command.scenarioPath().find<ScenarioModel>();

    // Create the end timenode
    ScenarioCreate<TimeNodeModel>::redo(
                m_newTimeNode,
                {{m_command.endStateY() - 0.2, m_command.endStateY() + 0.2}},
                m_date,
                scenar);

    // The event + state + constraint between
    m_command.redo();
}

void CreateConstraint_State_Event_TimeNode::serializeImpl(QDataStream& s) const
{
    s << m_newTimeNode << m_command.serialize() << m_date;
}

void CreateConstraint_State_Event_TimeNode::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> m_newTimeNode >> b >> m_date;

    m_command.deserialize(b);
}
