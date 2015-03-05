#include "MergeTimeNodes.hpp"

#include "source/Process/ScenarioModel.hpp"

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
        movingTimeNode->addEvent(event);
        scenar->event(event)->setDate(movingTimeNode->date());
        scenar->event(event)->changeTimeNode(m_aimedTimeNodeId);
        aimedTimeNode->removeEvent(event);
    }
}

void MergeTimeNodes::redo()
{
    auto scenar = m_path.find<ScenarioModel>();

    auto aimedTimeNode = scenar->timeNode(m_aimedTimeNodeId);
    auto movingTimeNode = scenar->timeNode(m_movingTimeNodeId);

    for (auto event : movingTimeNode->events())
    {
        scenar->event(event)->setDate(aimedTimeNode->date());
        aimedTimeNode->addEvent(event);
        scenar->event(event)->changeTimeNode(m_aimedTimeNodeId);
        movingTimeNode->removeEvent(event);
    }
    scenar->removeTimeNode(m_movingTimeNodeId);
}

bool MergeTimeNodes::mergeWith(const Command *other)
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
