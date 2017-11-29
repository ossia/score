// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <vector>

#include "SplitTimeSync.hpp"
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{

SplitTimeSync::SplitTimeSync(
    const TimeSyncModel& path, QVector<Id<EventModel>> eventsInNewTimeSync)
    : m_path{path}
    , m_eventsInNewTimeSync(std::move(eventsInNewTimeSync))
{
  m_originalTimeSyncId = path.id();

  auto scenar = static_cast<Scenario::ProcessModel*>(path.parent());
  m_newTimeSyncId = getStrongId(scenar->timeSyncs);
}

void SplitTimeSync::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());
  auto& originalTN = scenar.timeSync(m_originalTimeSyncId);
  auto& newTN = scenar.timeSync(m_newTimeSyncId);

  auto events = newTN.events(); // Copy to prevent iterator invalidation
  for (const auto& eventId : events)
  {
    newTN.removeEvent(eventId);
    originalTN.addEvent(eventId);
  }

  ScenarioCreate<TimeSyncModel>::undo(m_newTimeSyncId, scenar);

  updateTimeSyncExtent(originalTN.id(), scenar);
}

void SplitTimeSync::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());
  auto& originalTN = scenar.timeSync(m_originalTimeSyncId);

  // TODO set the correct position here.
  TimeSyncModel& tn = ScenarioCreate<TimeSyncModel>::redo(
      m_newTimeSyncId,
      VerticalExtent{}, // TODO
      originalTN.date(),
      scenar);

  tn.expression() = originalTN.expression();
  tn.setActive(originalTN.active());

  for (const auto& eventId : m_eventsInNewTimeSync)
  {
    originalTN.removeEvent(eventId);
    tn.addEvent(eventId);
  }

  updateTimeSyncExtent(originalTN.id(), scenar);
  updateTimeSyncExtent(tn.id(), scenar);
}

void SplitTimeSync::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_originalTimeSyncId << m_eventsInNewTimeSync
    << m_newTimeSyncId;
}

void SplitTimeSync::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_originalTimeSyncId >> m_eventsInNewTimeSync
      >> m_newTimeSyncId;
}
}
}
