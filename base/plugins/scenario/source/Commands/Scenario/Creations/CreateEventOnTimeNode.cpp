#include "CreateEventOnTimeNode.hpp"

#include "Document/Event/EventModel.hpp"
#include "source/Process/ScenarioModel.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;


CreateEventOnTimeNode::CreateEventOnTimeNode(ObjectPath&& scenarioPath, id_type<TimeNodeModel> timeNodeId, double heightPosition):
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(scenarioPath) },
    m_timeNodeId {timeNodeId},
    m_heightPosition {heightPosition}
{

}

void CreateEventOnTimeNode::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    StandardRemovalPolicy::removeEvent(*scenar, m_createdEventId);
}

void CreateEventOnTimeNode::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    m_createdEvent = getStrongId(scenar->events());

    EventModel* event = new EventModel{
                            m_createdEvent,
                            m_heightPosition,
                            &scenar };
    event->setDate(scenar->timeNode(m_timeNodeId)->date());

    scenar->addEvent(event);
}

bool CreateEventOnTimeNode::mergeWith(const Scenario::Command *other)
{
    return false;
}

void CreateEventOnTimeNode::serializeImpl(QDataStream &s) const
{
    s << m_path << m_timeNodeId << m_heightPosition;
}

void CreateEventOnTimeNode::deserializeImpl(QDataStream &s)
{
    s >> m_path >> m_timeNodeId >> m_heightPosition;
}

