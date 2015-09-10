#include "SplitEvent.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include "Tools/RandomNameProvider.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include "Process/Algorithms/StandardCreationPolicy.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"

using namespace Scenario::Command;


SplitEvent::SplitEvent(const Path<ScenarioModel> &scenario,
                       const Id<EventModel> &event,
                       const QVector<Id<StateModel>> &movingstates):
    iscore::SerializableCommand{"ScenarioConstrol", commandName(), description()},
    m_scenarioPath{scenario},
    m_originalEvent{event},
    m_newEvent{getStrongId(m_scenarioPath.find().events)},
    m_createdName{RandomNameProvider::generateRandomName()},
    m_movingStates{movingstates}
{

}

void SplitEvent::undo()
{
    auto& scenar = m_scenarioPath.find();
    auto& originalEvent = scenar.event(m_originalEvent);

    for (auto& st : m_movingStates)
    {
        originalEvent.addState(st);
        scenar.state(st).setEventId(m_originalEvent);
    }

    ScenarioCreate<EventModel>::undo(
                m_newEvent,
                m_scenarioPath.find());
}

void SplitEvent::redo()
{
    auto& scenar = m_scenarioPath.find();
    auto& originalEvent = scenar.event(m_originalEvent);
    ScenarioCreate<EventModel>::redo(
                m_newEvent,
                scenar.timeNode(originalEvent.timeNode()),
                originalEvent.extent(),
                scenar);

    auto& newEvent = scenar.event(m_newEvent);
    newEvent.metadata.setName(m_createdName);

    for(auto& st : m_movingStates)
    {
        originalEvent.removeState(st);
        newEvent.addState(st);
        scenar.state(st).setEventId(m_newEvent);
    }
}

void SplitEvent::serializeImpl(QDataStream & s) const
{
    s << m_scenarioPath
      << m_originalEvent
      << m_newEvent
      << m_createdName
      << m_movingStates;
}

void SplitEvent::deserializeImpl(QDataStream & s)
{
    s >> m_scenarioPath
      >> m_originalEvent
      >> m_newEvent
      >> m_createdName
      >> m_movingStates;
}
