// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "SplitTimeNode.hpp"
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{

SplitTimeNode::SplitTimeNode(
    const TimeNodeModel& path, QVector<Id<EventModel>> eventsInNewTimeNode)
    : m_path{path}
    , m_eventsInNewTimeNode(std::move(eventsInNewTimeNode))
{
  m_originalTimeNodeId = path.id();

  auto scenar = static_cast<Scenario::ProcessModel*>(path.parent());
  m_newTimeNodeId = getStrongId(scenar->timeNodes);
}

void SplitTimeNode::undo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());
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

void SplitTimeNode::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());
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

void SplitTimeNode::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_originalTimeNodeId << m_eventsInNewTimeNode
    << m_newTimeNodeId;
}

void SplitTimeNode::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_originalTimeNodeId >> m_eventsInNewTimeNode
      >> m_newTimeNodeId;
}
}
}
