#include "SplitTimeNode.hpp"

#include "source/Process/ScenarioModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Document/Event/EventModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include "Process/Algorithms/VerticalMovePolicy.hpp"
using namespace iscore;
using namespace Scenario::Command;


SplitTimeNode::SplitTimeNode(
        Path<TimeNodeModel>&& path,
        QVector<Id<EventModel> > eventsInNewTimeNode):
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_path {std::move(path) },
    m_eventsInNewTimeNode {(eventsInNewTimeNode) }
{
    auto& originalTN = m_path.find();
    m_originalTimeNodeId = originalTN.id();

    auto scenar = static_cast<ScenarioModel*>(originalTN.parent());
    m_newTimeNodeId = getStrongId(scenar->timeNodes);
}

void SplitTimeNode::undo() const
{
    auto& scenar = static_cast<ScenarioModel&>(*m_path.find().parent());
    auto& originalTN = scenar.timeNode(m_originalTimeNodeId);
    auto& newTN = scenar.timeNode(m_newTimeNodeId);

    auto events = newTN.events(); // Copy to prevent iterator invalidation
    for (const auto& eventId : events)
    {
        newTN.removeEvent(eventId);
        originalTN.addEvent(eventId);
    }

    ScenarioCreate<TimeNodeModel>::undo(m_newTimeNodeId, scenar);

    updateTimeNodeExtent(originalTN.id(), scenar);
}

void SplitTimeNode::redo() const
{
    auto& scenar = static_cast<ScenarioModel&>(*m_path.find().parent());
    auto& originalTN = scenar.timeNode(m_originalTimeNodeId);

    // TODO set the correct position here.
    auto& tn = ScenarioCreate<TimeNodeModel>::redo(
                m_newTimeNodeId,
                VerticalExtent{}, // TODO
                originalTN.date(),
                scenar);

    for (const auto& eventId : m_eventsInNewTimeNode)
    {
        originalTN.removeEvent(eventId);
        tn.addEvent(eventId);
    }

    updateTimeNodeExtent(originalTN.id(), scenar);
    updateTimeNodeExtent(tn.id(), scenar);
}

void SplitTimeNode::serializeImpl(QDataStream & s) const
{
    s << m_path << m_originalTimeNodeId << m_eventsInNewTimeNode << m_newTimeNodeId ;
}

void SplitTimeNode::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_originalTimeNodeId >> m_eventsInNewTimeNode << m_newTimeNodeId ;
}
