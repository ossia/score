#include "MergeTimeNodes.hpp"

#include "Scenario/Process/ScenarioModel.hpp"
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/Algorithms/StandardRemovalPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>

using namespace iscore;
using namespace Scenario::Command;

MergeTimeNodes::MergeTimeNodes( Path<ScenarioModel> &&path,
                                Id<TimeNodeModel> aimedTimeNode,
                                Id<TimeNodeModel> movingTimeNode):
    m_path {std::move(path) },
    m_aimedTimeNodeId{aimedTimeNode},
    m_movingTimeNodeId{movingTimeNode}
{
    auto& scenar = m_path.find();
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    s.readFrom(scenar.timeNode(m_movingTimeNodeId));
    m_serializedTimeNode = arr;
}

void MergeTimeNodes::undo() const
{

    ISCORE_TODO;
    /*
    auto& scenar = m_path.find();

    auto& aimedTimeNode = scenar.timeNode(m_aimedTimeNodeId);

    Deserializer<DataStream> s {m_serializedTimeNode};

    // todo make a function to do this (inline).
    auto movingTimeNode = new TimeNodeModel{s, &scenar};
    scenar.addTimeNode(movingTimeNode);

    for (auto& event : movingTimeNode->events())
    {
        aimedTimeNode.removeEvent(event);
        scenar.event(event).changeTimeNode(*movingTimeNode);
        StandardDisplacementPolicy::setEventPosition(scenar,
                                                     event,
                                                     movingTimeNode->date(),
                                                     scenar.event(event).heightPercentage(),
                                                     [] (ProcessModel* p, TimeValue t) { p->setDurationAndScale(t); });
    }
    */
}

void MergeTimeNodes::redo() const
{

    ISCORE_TODO;
    /*
    auto& scenar = m_path.find();

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
        scenar.event(event).changeTimeNode(aimedTimeNode);
    }

    CreateTimeNodeMin::undo(m_movingTimeNodeId, scenar);
    */
}

void MergeTimeNodes::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_aimedTimeNodeId << m_movingTimeNodeId << m_serializedTimeNode;
}

void MergeTimeNodes::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_aimedTimeNodeId >> m_movingTimeNodeId >> m_serializedTimeNode;
}
