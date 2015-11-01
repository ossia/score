#include "CreateConstraint_State_Event.hpp"

#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Scenario/Tools/RandomNameProvider.hpp>

using namespace Scenario::Command;
CreateConstraint_State_Event::CreateConstraint_State_Event(
        const ScenarioModel &scenario,
        const Id<StateModel>& startState,
        const Id<TimeNodeModel>& endTimeNode,
        double endStateY):
    iscore::SerializableCommand{factoryName(), commandName(), description()},
    m_newEvent{getStrongId(scenario.events)},
    m_createdName{RandomNameProvider::generateRandomName()},
    m_command{scenario,
              startState,
              m_newEvent,
              endStateY},
    m_endTimeNode{endTimeNode}
{

}

CreateConstraint_State_Event::CreateConstraint_State_Event(
        const Path<ScenarioModel> &scenarioPath,
        const Id<StateModel> &startState,
        const Id<TimeNodeModel> &endTimeNode,
        double endStateY):
    CreateConstraint_State_Event{
        scenarioPath.find(),
        startState,
        endTimeNode,
        endStateY}
{

}

void CreateConstraint_State_Event::undo() const
{
    m_command.undo();

    ScenarioCreate<EventModel>::undo(
                m_newEvent,
                m_command.scenarioPath().find());
}

void CreateConstraint_State_Event::redo() const
{
    auto& scenar = m_command.scenarioPath().find();

    // Create the end event
    ScenarioCreate<EventModel>::redo(
                m_newEvent,
                scenar.timeNode(m_endTimeNode),
                {m_command.endStateY(), m_command.endStateY()},
                scenar);

    scenar.events.at(m_newEvent).metadata.setName(m_createdName);

    // The state + constraint between
    m_command.redo();
}

void CreateConstraint_State_Event::serializeImpl(QDataStream& s) const
{
    s << m_newEvent
      << m_createdName
      << m_command.serialize()
      << m_endTimeNode;
}

void CreateConstraint_State_Event::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> m_newEvent >> m_createdName >> b >> m_endTimeNode;

    m_command.deserialize(b);
}
