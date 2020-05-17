// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MoveNewEvent.hpp"

#include <Scenario/Commands/Scenario/Displacement/MoveEventOnCreationMeta.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <QByteArray>

namespace Scenario
{
namespace Command
{
MoveNewEvent::MoveNewEvent(
    const Scenario::ProcessModel& scenarioPath,
    Id<IntervalModel> intervalId,
    Id<EventModel> eventId,
    TimeVal date,
    double y,
    bool yLocked)
    : m_path{scenarioPath}
    , m_intervalId{std::move(intervalId)}
    , m_cmd{scenarioPath, std::move(eventId), std::move(date), ExpandMode::Scale}
    , m_y{y}
    , m_yLocked{yLocked}
{
}

MoveNewEvent::MoveNewEvent(
    const Scenario::ProcessModel& scenarioPath,
    Id<IntervalModel> intervalId,
    Id<EventModel> eventId,
    TimeVal date,
    const double y,
    bool yLocked,
    ExpandMode mode)
    : m_path{scenarioPath}
    , m_intervalId{std::move(intervalId)}
    , m_cmd{scenarioPath, std::move(eventId), std::move(date), mode}
    , m_y{y}
    , m_yLocked{yLocked}
{
}

void MoveNewEvent::undo(const score::DocumentContext& ctx) const
{
  m_cmd.undo(ctx);
  // It is not needed to move the interval since
  // the sub-command already does it correctly.
}

void MoveNewEvent::redo(const score::DocumentContext& ctx) const
{
  m_cmd.redo(ctx);
  if (!m_yLocked)
  {
    auto& scenar = m_cmd.path().find(ctx);
    auto& itv = scenar.intervals.at(m_intervalId);
    if (itv.graphal())
    {
      scenar.states.at(itv.endState()).setHeightPercentage(m_y);
    }
    itv.requestHeightChange(m_y);
  }
}

void MoveNewEvent::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cmd.serialize() << m_intervalId << m_y << m_yLocked;
}

void MoveNewEvent::deserializeImpl(DataStreamOutput& s)
{
  QByteArray a;
  s >> m_path >> a >> m_intervalId >> m_y >> m_yLocked;

  m_cmd.deserialize(a);
}
}
}
