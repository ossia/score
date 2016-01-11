#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/RandomNameProvider.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <vector>

#include <Process/ModelMetadata.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include "SplitEvent.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>

namespace Scenario
{
namespace Command
{
SplitEvent::SplitEvent(const Path<Scenario::ScenarioModel> &scenario,
                       const Id<EventModel> &event,
                       const QVector<Id<StateModel>> &movingstates):
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

void SplitEvent::serializeImpl(DataStreamInput & s) const
{
    s << m_scenarioPath
      << m_originalEvent
      << m_newEvent
      << m_createdName
      << m_movingStates;
}

void SplitEvent::deserializeImpl(DataStreamOutput & s)
{
    s >> m_scenarioPath
      >> m_originalEvent
      >> m_newEvent
      >> m_createdName
      >> m_movingStates;
}
}
}
