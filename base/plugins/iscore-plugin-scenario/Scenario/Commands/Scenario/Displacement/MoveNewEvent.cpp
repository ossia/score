// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QByteArray>
#include <algorithm>

#include "MoveNewEvent.hpp"
#include <Scenario/Commands/Scenario/Displacement/MoveEventOnCreationMeta.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
MoveNewEvent::MoveNewEvent(
    const Scenario::ProcessModel& scenarioPath,
    Id<ConstraintModel> constraintId,
    Id<EventModel> eventId,
    TimeVal date,
    double y,
    bool yLocked)
    : m_path{scenarioPath}
    , m_constraintId{std::move(constraintId)}
    , m_cmd{scenarioPath, std::move(eventId), std::move(date), ExpandMode::Scale}
    , m_y{y}
    , m_yLocked{yLocked}
{
}

MoveNewEvent::MoveNewEvent(
    const Scenario::ProcessModel& scenarioPath,
    Id<ConstraintModel> constraintId,
    Id<EventModel> eventId,
    TimeVal date,
    const double y,
    bool yLocked,
    ExpandMode mode)
    : m_path{scenarioPath}
    , m_constraintId{std::move(constraintId)}
    , m_cmd{scenarioPath, std::move(eventId), std::move(date), mode}
    , m_y{y}
    , m_yLocked{yLocked}
{
}

void MoveNewEvent::undo(const iscore::DocumentContext& ctx) const
{
  m_cmd.undo(ctx);
  // It is not needed to move the constraint since
  // the sub-command already does it correctly.
}

void MoveNewEvent::redo(const iscore::DocumentContext& ctx) const
{
  m_cmd.redo(ctx);
  if (!m_yLocked)
  {
    updateConstraintVerticalPos(m_y, m_constraintId, m_cmd.path().find(ctx));
  }
}

void MoveNewEvent::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cmd.serialize() << m_constraintId << m_y << m_yLocked;
}

void MoveNewEvent::deserializeImpl(DataStreamOutput& s)
{
  QByteArray a;
  s >> m_path >> a >> m_constraintId >> m_y >> m_yLocked;

  m_cmd.deserialize(a);
}
}
}
