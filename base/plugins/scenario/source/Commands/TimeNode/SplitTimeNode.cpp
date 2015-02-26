#include "SplitTimeNode.hpp"

#include "source/Process/ScenarioModel.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"

Scenario::Command::SplitTimeNode::SplitTimeNode(ObjectPath &&path, QVector<id_type<EventModel> > eventsInNewTimeNode):
    SerializableCommand {"ScenarioControl",
        "Split TimeNode in two",
         QObject::tr("Split TimeNode in two")},
    m_path {std::move(path) },
    m_eventsInNewTimeNode {(eventsInNewTimeNode) }
{
    auto originalTimeNodeModel = m_path.find<TimeNodeModel>();
    m_originalTimeNodeId = originalTimeNodeModel->id();

    auto scenar = static_cast<ScenarioModel*>(originalTimeNodeModel->parent());
    m_newTimeNodeId = getStrongId(scenar->timeNodes());
}

void Scenario::Command::SplitTimeNode::undo()
{
    auto scenar = static_cast<ScenarioModel*>(m_path.find<TimeNodeModel>()->parent());
    auto originalTN = scenar->timeNode(m_originalTimeNodeId);
    auto newTN = scenar->timeNode(m_newTimeNodeId);

    for (auto eventId : newTN->events())
    {
        originalTN->addEvent(eventId);
        scenar->event(eventId)->changeTimeNode(m_originalTimeNodeId);
        newTN->removeEvent(eventId);
    }
    scenar->removeTimeNode(m_newTimeNodeId);
}

void Scenario::Command::SplitTimeNode::redo()
{
    auto scenar = static_cast<ScenarioModel*>(m_path.find<TimeNodeModel>()->parent());
    auto originalTN = scenar->timeNode(m_originalTimeNodeId);

    auto newTimeNode = new TimeNodeModel{m_newTimeNodeId, originalTN->date(), scenar};
    scenar->addTimeNode(newTimeNode);

    for (auto eventId : m_eventsInNewTimeNode)
    {
        newTimeNode->addEvent(eventId);
        originalTN->removeEvent(eventId);
        auto event = scenar->event(eventId);
        event->changeTimeNode(m_newTimeNodeId);
    }

}

int Scenario::Command::SplitTimeNode::id() const
{
    return 1;
}

bool Scenario::Command::SplitTimeNode::mergeWith(const QUndoCommand *other)
{
    return false;
}

void Scenario::Command::SplitTimeNode::serializeImpl(QDataStream & s) const
{
    s << m_path << m_originalTimeNodeId << m_eventsInNewTimeNode << m_newTimeNodeId ;
}

void Scenario::Command::SplitTimeNode::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_originalTimeNodeId >> m_eventsInNewTimeNode << m_newTimeNodeId ;
}
