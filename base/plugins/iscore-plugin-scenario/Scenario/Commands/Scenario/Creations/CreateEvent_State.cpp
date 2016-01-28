#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/RandomNameProvider.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>
#include <vector>

#include "CreateEvent_State.hpp"
#include <Process/ModelMetadata.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/NotifyingMap.hpp>


namespace Scenario
{
namespace Command
{
CreateEvent_State::CreateEvent_State(
        const Scenario::ScenarioModel& scenario,
        const Id<TimeNodeModel>& timeNode,
        double stateY):
    m_newEvent{getStrongId(scenario.events)},
    m_createdName{RandomNameProvider::generateRandomName()},
    m_command{scenario,
              m_newEvent,
              stateY},
    m_timeNode{timeNode}
{

}

CreateEvent_State::CreateEvent_State(
        const Path<Scenario::ScenarioModel>& scenario,
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
                {m_command.endStateY() - 0.1, m_command.endStateY() + 0.1},
                scenar);

    scenar.events.at(m_newEvent).metadata.setName(m_createdName);

    // And the state
    m_command.redo();
}

void CreateEvent_State::serializeImpl(DataStreamInput& s) const
{
    s << m_newEvent
      << m_createdName
      << m_command.serialize()
      << m_timeNode;
}

void CreateEvent_State::deserializeImpl(DataStreamOutput& s)
{
    QByteArray b;
    s >> m_newEvent
            >> m_createdName
            >> b
            >> m_timeNode;

    m_command.deserialize(b);
}
}
}
