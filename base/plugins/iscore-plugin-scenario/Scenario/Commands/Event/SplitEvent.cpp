#include "SplitEvent.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Scenario/Tools/RandomNameProvider.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>

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

void SplitEvent::undo() const
{
    auto& scenar = m_scenarioPath.find();
    auto& originalEvent = scenar.events.at(m_originalEvent);

    for (auto& st : m_movingStates)
    {
        originalEvent.addState(st);
        scenar.states.at(st).setEventId(m_originalEvent);
    }

    ScenarioCreate<EventModel>::undo(
                m_newEvent,
                m_scenarioPath.find());

    updateEventExtent(m_originalEvent, scenar);
}

void SplitEvent::redo() const
{
    auto& scenar = m_scenarioPath.find();
    auto& originalEvent = scenar.event(m_originalEvent);
    ScenarioCreate<EventModel>::redo(
                m_newEvent,
                scenar.timeNodes.at(originalEvent.timeNode()),
                originalEvent.extent(),
                scenar);

    auto& newEvent = scenar.events.at(m_newEvent);
    newEvent.metadata.setName(m_createdName);

    for(auto& st : m_movingStates)
    {
        originalEvent.removeState(st);
        newEvent.addState(st);
        scenar.states.at(st).setEventId(m_newEvent);
    }

    updateEventExtent(m_newEvent, scenar);
    updateEventExtent(m_originalEvent, scenar);
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
