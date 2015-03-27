#include "SplitTimeNode.hpp"

#include "source/Process/ScenarioModel.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;


SplitTimeNode::SplitTimeNode(ObjectPath &&path, QVector<id_type<EventModel> > eventsInNewTimeNode):
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
    m_path {std::move(path) },
    m_eventsInNewTimeNode {(eventsInNewTimeNode) }
{
    auto originalTimeNodeModel = m_path.find<TimeNodeModel>();
    m_originalTimeNodeId = originalTimeNodeModel->id();

    auto scenar = static_cast<ScenarioModel*>(originalTimeNodeModel->parent());
    m_newTimeNodeId = getStrongId(scenar->timeNodes());
}

void SplitTimeNode::undo()
{
    auto scenar = static_cast<ScenarioModel*>(m_path.find<TimeNodeModel>()->parent());
    auto originalTN = scenar->timeNode(m_originalTimeNodeId);
    auto newTN = scenar->timeNode(m_newTimeNodeId);

    for (auto eventId : newTN->events())
    {
        originalTN->addEvent(eventId);
        newTN->removeEvent(eventId);
        scenar->event(eventId)->changeTimeNode(m_originalTimeNodeId);
    }

    StandardRemovalPolicy::removeTimeNode(*scenar, m_newTimeNodeId);
}

void SplitTimeNode::redo()
{
    auto scenar = static_cast<ScenarioModel*>(m_path.find<TimeNodeModel>()->parent());
    auto originalTN = scenar->timeNode(m_originalTimeNodeId);

    auto newTimeNode = new TimeNodeModel{m_newTimeNodeId, originalTN->date(), scenar};
    scenar->addTimeNode(newTimeNode);

    for (auto eventId : m_eventsInNewTimeNode)
    {
        newTimeNode->addEvent(eventId);
        originalTN->removeEvent(eventId);
        scenar->event(eventId)->changeTimeNode(m_newTimeNodeId);
    }

}

bool SplitTimeNode::mergeWith(const Command *other)
{
    return false;
}

void SplitTimeNode::serializeImpl(QDataStream & s) const
{
    s << m_path << m_originalTimeNodeId << m_eventsInNewTimeNode << m_newTimeNodeId ;
}

void SplitTimeNode::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_originalTimeNodeId >> m_eventsInNewTimeNode << m_newTimeNodeId ;
}
