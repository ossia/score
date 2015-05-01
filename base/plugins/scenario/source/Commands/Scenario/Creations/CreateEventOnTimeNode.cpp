#include "CreateEventOnTimeNode.hpp"

#include "Document/Event/EventModel.hpp"
#include "source/Process/ScenarioModel.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;


CreateEventOnTimeNode::CreateEventOnTimeNode(
        ObjectPath&& scenarioPath,
        id_type<TimeNodeModel> timeNodeId,
        double heightPosition):
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(scenarioPath) },
    m_timeNodeId {timeNodeId},
    m_heightPosition {heightPosition}
{
    auto scenar = m_path.find<ScenarioModel>();
    m_createdEventId = getStrongId(scenar->events());
}

void CreateEventOnTimeNode::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    StandardRemovalPolicy::removeEventAndConstraints(
                *scenar,
                m_createdEventId);
}

void CreateEventOnTimeNode::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    CreateEventMin::redo(
                m_createdEventId,
                *scenar->timeNode(m_timeNodeId),
                m_heightPosition,
                *scenar);
}

void CreateEventOnTimeNode::serializeImpl(QDataStream &s) const
{
    s << m_path
      << m_timeNodeId
      << m_heightPosition
      << m_createdEventId;
}

void CreateEventOnTimeNode::deserializeImpl(QDataStream &s)
{
    s >> m_path
      >> m_timeNodeId
      >> m_heightPosition
      >> m_createdEventId;
}

