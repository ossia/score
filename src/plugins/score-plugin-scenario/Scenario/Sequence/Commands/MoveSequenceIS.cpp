// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MoveSequenceIS.hpp"

#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Process/ExpandMode.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

namespace Sequence
{
namespace Command
{

MoveSequenceIS::MoveSequenceIS(
    const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId, TimeVal newDate)
    : m_path{seq}
    , m_tsId{tsId}
    , m_oldDate{seq.timeSyncs.at(tsId).date()}
    , m_newDate{newDate}
{
}

void MoveSequenceIS::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).moveIS(m_tsId, m_oldDate);
}

void MoveSequenceIS::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).moveIS(m_tsId, m_newDate);
}

void MoveSequenceIS::update(
    const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId, TimeVal newDate)
{
  m_newDate = newDate;
}

void MoveSequenceIS::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_tsId << m_oldDate << m_newDate;
}

void MoveSequenceIS::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_tsId >> m_oldDate >> m_newDate;
}

MoveSequenceISRipple::MoveSequenceISRipple(
    const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId, TimeVal newDate)
    : m_path{seq}
    , m_tsId{tsId}
    , m_oldDate{seq.timeSyncs.at(tsId).date()}
    , m_newDate{newDate}
{
  // The parent interval's end event moves by the same delta so the sequence
  // and its container stay in sync.
  if(auto* itv = qobject_cast<Scenario::IntervalModel*>(seq.parent()))
  {
    if(auto* scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent()))
    {
      auto& endSt = scenar->states.at(itv->endState());
      m_endEventId = endSt.eventId();
      m_endY = endSt.heightPercentage();
      m_origParentEnd = scenar->events.at(m_endEventId).date();
      const TimeVal delta = m_newDate - m_oldDate;
      m_moveCmd = std::make_unique<Scenario::Command::MoveEventMeta>(
          *scenar, m_endEventId, m_origParentEnd + delta, m_endY,
          ExpandMode::GrowShrink, LockMode::Free);
    }
  }
}

MoveSequenceISRipple::~MoveSequenceISRipple() = default;

void MoveSequenceISRipple::update(
    const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId, TimeVal newDate)
{
  m_newDate = newDate;
  if(m_moveCmd)
  {
    if(auto* itv = qobject_cast<Scenario::IntervalModel*>(seq.parent()))
    {
      if(auto* scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent()))
      {
        m_moveCmd->update(
            *scenar, m_endEventId, m_origParentEnd + (m_newDate - m_oldDate), m_endY,
            ExpandMode::GrowShrink, LockMode::Free);
      }
    }
  }
}

void MoveSequenceISRipple::redo(const score::DocumentContext& ctx) const
{
  // Ripple first: the sequence duration then already matches the new parent
  // duration, so setDurationAndGrow/Shrink is a no-op when the event moves.
  m_path.find(ctx).moveISRipple(m_tsId, m_newDate);
  if(m_moveCmd)
    m_moveCmd->redo(ctx);
}

void MoveSequenceISRipple::undo(const score::DocumentContext& ctx) const
{
  // Reverse the ripple on the live model first; MoveEvent::undo then restores
  // the parent's processes from its pre-gesture snapshot (consistent state).
  m_path.find(ctx).moveISRipple(m_tsId, m_oldDate);
  if(m_moveCmd)
    m_moveCmd->undo(ctx);
}

void MoveSequenceISRipple::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_tsId << m_oldDate << m_newDate << m_origParentEnd << m_endEventId
    << m_endY;
  s << bool(m_moveCmd);
  if(m_moveCmd)
    s << m_moveCmd->serialize();
}

void MoveSequenceISRipple::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_tsId >> m_oldDate >> m_newDate >> m_origParentEnd >> m_endEventId
      >> m_endY;
  bool hasMove{};
  s >> hasMove;
  if(hasMove)
  {
    QByteArray moveData;
    s >> moveData;
    m_moveCmd = std::make_unique<Scenario::Command::MoveEventMeta>();
    m_moveCmd->deserialize(moveData);
  }
}

InsertSequenceIS::InsertSequenceIS(const SequenceModel& seq, TimeVal date)
    : m_path{seq}
    , m_date{date}
{
  if(auto sec = seq.sectionAt(date))
  {
    // Pre-allocate IDs so they stay stable across undo/redo cycles.
    m_info.leftItvId = *sec;
    m_info.newTsId = getStrongId(seq.timeSyncs);
    m_info.newEvId = getStrongId(seq.events);
    m_info.newStId = getStrongId(seq.states);
    m_info.rightItvId = getStrongId(seq.intervals);
    m_valid = true;
  }
}

void InsertSequenceIS::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).undoInsertIS(m_info);
}

void InsertSequenceIS::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).insertISWithIds(m_info, m_date);
}

void InsertSequenceIS::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_date << m_info.newTsId << m_info.newEvId << m_info.newStId
    << m_info.rightItvId << m_info.leftItvId << m_valid;
}

void InsertSequenceIS::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_date >> m_info.newTsId >> m_info.newEvId >> m_info.newStId
      >> m_info.rightItvId >> m_info.leftItvId >> m_valid;
}

}
}
