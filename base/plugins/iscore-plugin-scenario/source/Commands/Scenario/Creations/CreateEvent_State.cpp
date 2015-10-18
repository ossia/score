#include "CreateEvent_State.hpp"

#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/ScenarioModel.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include "Tools/RandomNameProvider.hpp"

using namespace Scenario::Command;
CreateEvent_State::CreateEvent_State(
        const ScenarioModel& scenario,
        const Id<TimeNodeModel>& timeNode,
        double stateY):
    iscore::SerializableCommand{"ScenarioControl", commandName(), description()},
    m_newEvent{getStrongId(scenario.events)},
    m_createdName{RandomNameProvider::generateRandomName()},
    m_command{scenario,
              m_newEvent,
              stateY},
    m_timeNode{timeNode}
{

}

CreateEvent_State::CreateEvent_State(
        const Path<ScenarioModel>& scenario,
        const Id<TimeNodeModel> &timeNode,
        double stateY):
    CreateEvent_State{scenario.find(),
                      timeNode,
                      stateY}
{

}

void CreateEvent_State::undo() const
{
    m_command.undo();

    ScenarioCreate<EventModel>::undo(
                m_newEvent,
                m_command.scenarioPath().find());
}

void CreateEvent_State::redo() const
{
    auto& scenar = m_command.scenarioPath().find();

    // Create the event
    ScenarioCreate<EventModel>::redo(
                m_newEvent,
                scenar.timeNode(m_timeNode),
                {{m_command.endStateY() - 0.1, m_command.endStateY() + 0.1}},
                scenar);

    scenar.events.at(m_newEvent).metadata.setName(m_createdName);

    // And the state
    m_command.redo();
}

void CreateEvent_State::serializeImpl(QDataStream& s) const
{
    s << m_newEvent
      << m_createdName
      << m_command.serialize()
      << m_timeNode;
}

void CreateEvent_State::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> m_newEvent
            >> m_createdName
            >> b
            >> m_timeNode;

    m_command.deserialize(b);
}
