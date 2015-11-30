#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>
#include <vector>

#include "CreateTimeNode_Event_State.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent_State.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/NotifyingMap.hpp>

using namespace Scenario::Command;
CreateTimeNode_Event_State::CreateTimeNode_Event_State(
        const Scenario::ScenarioModel& scenario,
        const TimeValue& date,
        double stateY):
    m_newTimeNode{getStrongId(scenario.timeNodes)},
    m_date{date},
    m_command{scenario,
              m_newTimeNode,
              stateY}
{

}

CreateTimeNode_Event_State::CreateTimeNode_Event_State(
        const Path<Scenario::ScenarioModel>&scenario,
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
                {0.4, 0.6},
                m_date,
                scenar);

    // And the event
    m_command.redo();
}

void CreateTimeNode_Event_State::serializeImpl(DataStreamInput& s) const
{
    s << m_newTimeNode << m_date << m_command.serialize();
}

void CreateTimeNode_Event_State::deserializeImpl(DataStreamOutput& s)
{
    QByteArray b;
    s >> m_newTimeNode >> m_date >> b;

    m_command.deserialize(b);
}
