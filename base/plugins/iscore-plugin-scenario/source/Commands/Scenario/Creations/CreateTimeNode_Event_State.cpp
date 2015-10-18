#include "CreateTimeNode_Event_State.hpp"

#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/ScenarioModel.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace Scenario::Command;
CreateTimeNode_Event_State::CreateTimeNode_Event_State(
        const ScenarioModel& scenario,
        const TimeValue& date,
        double stateY):
    iscore::SerializableCommand{"ScenarioControl", commandName(), description()},
    m_newTimeNode{getStrongId(scenario.timeNodes)},
    m_date{date},
    m_command{scenario,
              m_newTimeNode,
              stateY}
{

}

CreateTimeNode_Event_State::CreateTimeNode_Event_State(
        const Path<ScenarioModel>&scenario,
        const TimeValue& date,
        double stateY):
    CreateTimeNode_Event_State{scenario.find(),
                      date,
                      stateY}
{

}

void CreateTimeNode_Event_State::undo() const
{
    m_command.undo();

    ScenarioCreate<TimeNodeModel>::undo(
                m_newTimeNode,
                m_command.scenarioPath().find());
}

void CreateTimeNode_Event_State::redo() const
{
    auto& scenar = m_command.scenarioPath().find();

    // Create the node
    ScenarioCreate<TimeNodeModel>::redo(
                m_newTimeNode,
                {{0.4, 0.6}},
                m_date,
                scenar);

    // And the event
    m_command.redo();
}

void CreateTimeNode_Event_State::serializeImpl(QDataStream& s) const
{
    s << m_newTimeNode << m_date << m_command.serialize();
}

void CreateTimeNode_Event_State::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> m_newTimeNode >> m_date >> b;

    m_command.deserialize(b);
}
