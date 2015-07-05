#include "SplitTimeNode.hpp"

#include "source/Process/ScenarioModel.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Document/Event/EventModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>
using namespace iscore;
using namespace Scenario::Command;


SplitTimeNode::SplitTimeNode(ObjectPath &&path, QVector<id_type<EventModel> > eventsInNewTimeNode):
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_path {std::move(path) },
    m_eventsInNewTimeNode {(eventsInNewTimeNode) }
{
    ISCORE_TODO
    /*
    auto& originalTN = m_path.find<TimeNodeModel>();
    m_originalTimeNodeId = originalTN.id();

    auto scenar = static_cast<ScenarioModel*>(originalTN.parent());
    m_newTimeNodeId = getStrongId(scenar->timeNodes());
    */
}

void SplitTimeNode::undo()
{
    ISCORE_TODO
    /*
    auto& scenar = static_cast<ScenarioModel&>(*m_path.find<TimeNodeModel>().parent());
    auto& originalTN = scenar.timeNode(m_originalTimeNodeId);
    auto& newTN = scenar.timeNode(m_newTimeNodeId);

    for (auto& eventId : newTN.events())
    {
        originalTN.addEvent(eventId);
        newTN.removeEvent(eventId);
        scenar.event(eventId).changeTimeNode(m_originalTimeNodeId);
    }

    CreateTimeNodeMin::undo(m_newTimeNodeId, scenar);
    */
}

void SplitTimeNode::redo()
{
    ISCORE_TODO
    /*
    auto& scenar = static_cast<ScenarioModel&>(*m_path.find<TimeNodeModel>().parent());
    auto& originalTN = scenar.timeNode(m_originalTimeNodeId);

    // TODO set the correct position here.
    auto& tn = CreateTimeNodeMin::redo(m_newTimeNodeId, originalTN.date(), scenar);

    for (auto& eventId : m_eventsInNewTimeNode)
    {
        tn.addEvent(eventId);
        originalTN.removeEvent(eventId);
        scenar.event(eventId).changeTimeNode(m_newTimeNodeId);
    }
    */
}

void SplitTimeNode::serializeImpl(QDataStream & s) const
{
    s << m_path << m_originalTimeNodeId << m_eventsInNewTimeNode << m_newTimeNodeId ;
}

void SplitTimeNode::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_originalTimeNodeId >> m_eventsInNewTimeNode << m_newTimeNodeId ;
}
