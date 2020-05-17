// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SplitTimeSync.hpp"

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <vector>

namespace Scenario
{
namespace Command
{

SplitTimeSync::SplitTimeSync(
    const TimeSyncModel& path,
    QVector<Id<EventModel>> eventsInNewTimeSync)
    : m_path{path}, m_eventsInNewTimeSync(std::move(eventsInNewTimeSync))
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
}

void SplitTimeSync::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());
  auto& originalTN = scenar.timeSync(m_originalTimeSyncId);

  // TODO set the correct position here.
  TimeSyncModel& tn
      = ScenarioCreate<TimeSyncModel>::redo(m_newTimeSyncId, originalTN.date(), scenar);

  tn.expression() = originalTN.expression();
  tn.setActive(originalTN.active());

  for (const auto& eventId : m_eventsInNewTimeSync)
  {
    originalTN.removeEvent(eventId);
    tn.addEvent(eventId);
  }
}

void SplitTimeSync::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_originalTimeSyncId << m_eventsInNewTimeSync << m_newTimeSyncId;
}

void SplitTimeSync::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_originalTimeSyncId >> m_eventsInNewTimeSync >> m_newTimeSyncId;
}

SplitWholeSync::SplitWholeSync(const TimeSyncModel& path) : m_path{path}
{
  SCORE_ASSERT(path.events().size() > 1);
  m_originalTimeSync = path.id();
  auto scenar = static_cast<Scenario::ProcessModel*>(path.parent());
  m_newTimeSyncs
      = getStrongIdRange<Scenario::TimeSyncModel>(path.events().size() - 1, scenar->timeSyncs);
}

SplitWholeSync::SplitWholeSync(const TimeSyncModel& path, std::vector<Id<TimeSyncModel>> new_ids)
    : m_path{path}, m_originalTimeSync{path.id()}, m_newTimeSyncs{std::move(new_ids)}
{
}

void SplitWholeSync::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());

  auto& originalTN = scenar.timeSync(m_originalTimeSync);
  for (const auto& id : m_newTimeSyncs)
  {
    auto& newTN = scenar.timeSync(id);

    SCORE_ASSERT(newTN.events().size() == 1);
    const auto& eventId = newTN.events().front();
    {
      newTN.removeEvent(eventId);
      originalTN.addEvent(eventId);
    }

    ScenarioCreate<TimeSyncModel>::undo(id, scenar);
  }
}

void SplitWholeSync::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());
  auto& originalTN = scenar.timeSync(m_originalTimeSync);

  auto originalEvents = originalTN.events();

  std::size_t k = 1;
  for (const auto& id : m_newTimeSyncs)
  {
    // TODO set the correct position here.
    TimeSyncModel& tn = ScenarioCreate<TimeSyncModel>::redo(id, originalTN.date(), scenar);

    tn.expression() = originalTN.expression();
    tn.setActive(originalTN.active());

    originalTN.removeEvent(originalEvents[k]);
    tn.addEvent(originalEvents[k]);

    k++;
  }
}

void SplitWholeSync::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_originalTimeSync << m_newTimeSyncs;
}

void SplitWholeSync::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_originalTimeSync >> m_newTimeSyncs;
}
}
}
