#include "MergeTimeNodes.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

MergeTimeNodes::MergeTimeNodes( ObjectPath &&path,
                                id_type<TimeNodeModel> aimedTimeNode,
                                id_type<TimeNodeModel> movingTimeNode):
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(path) },
    m_aimedTimeNodeId{aimedTimeNode},
    m_movingTimeNodeId{movingTimeNode}
{
    auto scenar = m_path.find<ScenarioModel>();
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    s.readFrom(*scenar->timeNode(m_movingTimeNodeId));
    m_serializedTimeNode = arr;
}

void MergeTimeNodes::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    auto aimedTimeNode = scenar->timeNode(m_aimedTimeNodeId);

    Deserializer<DataStream> s {&m_serializedTimeNode};
    auto movingTimeNode = new TimeNodeModel(s, scenar);
    scenar->addTimeNode(movingTimeNode);
    for (auto event : movingTimeNode->events())
    {
        aimedTimeNode->removeEvent(event);
        scenar->event(event)->changeTimeNode(movingTimeNode->id());
        StandardDisplacementPolicy::setEventPosition(*scenar, event, movingTimeNode->date(), scenar->event(event)->heightPercentage());
    }
}

void MergeTimeNodes::redo()
{
    auto scenar = m_path.find<ScenarioModel>();

    auto aimedTimeNode = scenar->timeNode(m_aimedTimeNodeId);
    auto movingTimeNode = scenar->timeNode(m_movingTimeNodeId);

    for (auto event : movingTimeNode->events())
    {
        StandardDisplacementPolicy::setEventPosition(*scenar, event, aimedTimeNode->date(), scenar->event(event)->heightPercentage());
        aimedTimeNode->addEvent(event);
        movingTimeNode->removeEvent(event);
    }
    scenar->removeTimeNode(m_movingTimeNodeId);
}

int MergeTimeNodes::id() const
{
    return 1;
}

bool MergeTimeNodes::mergeWith(const QUndoCommand *other)
{
    return false;
}

void MergeTimeNodes::serializeImpl(QDataStream & s) const
{
    s << m_path << m_aimedTimeNodeId << m_movingTimeNodeId << m_serializedTimeNode;
}

void MergeTimeNodes::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_aimedTimeNodeId >> m_movingTimeNodeId >> m_serializedTimeNode;
}
