#include "MergeTimeNodes.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

MergeTimeNodes::MergeTimeNodes( ObjectPath &&path,
                                id_type<TimeNodeModel> aimedTimeNode,
                                id_type<TimeNodeModel> movingTimeNode):
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(path) },
    m_aimedTimeNodeId{aimedTimeNode},
    m_movingTimeNodeId{movingTimeNode}
{
    auto& scenar = m_path.find<ScenarioModel>();
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    s.readFrom(scenar.timeNode(m_movingTimeNodeId));
    m_serializedTimeNode = arr;
}

void MergeTimeNodes::undo()
{

    ISCORE_TODO
    /*
    auto& scenar = m_path.find<ScenarioModel>();

    auto& aimedTimeNode = scenar.timeNode(m_aimedTimeNodeId);

    Deserializer<DataStream> s {m_serializedTimeNode};

    // todo make a function to do this (inline).
    auto movingTimeNode = new TimeNodeModel{s, &scenar};
    scenar.addTimeNode(movingTimeNode);

    for (auto& event : movingTimeNode->events())
    {
        aimedTimeNode.removeEvent(event);
        scenar.event(event).changeTimeNode(movingTimeNode->id());
        StandardDisplacementPolicy::setEventPosition(scenar,
                                                     event,
                                                     movingTimeNode->date(),
                                                     scenar.event(event).heightPercentage(),
                                                     [] (ProcessModel* p, TimeValue t) { p->setDurationAndScale(t); });
    }
    */
}

void MergeTimeNodes::redo()
{

    ISCORE_TODO
    /*
    auto& scenar = m_path.find<ScenarioModel>();

    auto& aimedTimeNode = scenar.timeNode(m_aimedTimeNodeId);
    auto& movingTimeNode = scenar.timeNode(m_movingTimeNodeId);

    for (const auto& event : movingTimeNode.events())
    {
        StandardDisplacementPolicy::setEventPosition(
                    scenar,
                    event,
                    aimedTimeNode.date(),
                    scenar.event(event).heightPercentage(),
                    [] (ProcessModel* p, TimeValue t)
                        { p->setDurationAndScale(t); });

        aimedTimeNode.addEvent(event);
        movingTimeNode.removeEvent(event);
        scenar.event(event).changeTimeNode(aimedTimeNode.id());
    }

    CreateTimeNodeMin::undo(m_movingTimeNodeId, scenar);
    */
}

void MergeTimeNodes::serializeImpl(QDataStream & s) const
{
    s << m_path << m_aimedTimeNodeId << m_movingTimeNodeId << m_serializedTimeNode;
}

void MergeTimeNodes::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_aimedTimeNodeId >> m_movingTimeNodeId >> m_serializedTimeNode;
}
