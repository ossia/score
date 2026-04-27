// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExtendSequence.hpp"

#include "AppendSequenceSection.hpp"

#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <Process/ExpandMode.hpp>

#include <score/serialization/DataStreamVisitor.hpp>

namespace Sequence
{
namespace Command
{

ExtendSequence::ExtendSequence(
    const Scenario::ProcessModel& scenario,
    const Id<Scenario::StateModel>& endStateId, const TimeVal& newDate, double y)
{
  auto& endState = scenario.states.at(endStateId);
  SCORE_ASSERT(endState.previousInterval());
  auto& parentItv = scenario.intervals.at(*endState.previousInterval());

  auto* seqModel = Sequence::findSequenceProcess(parentItv);
  SCORE_ASSERT(seqModel);

  m_endEventId = endState.eventId();
  const auto& endEvent = scenario.events.at(m_endEventId);
  const TimeVal sectionDuration = newDate - endEvent.date();
  SCORE_ASSERT(sectionDuration > TimeVal::zero());

  // AppendSection first so SequenceModel.duration() == newDate when MoveEventMeta runs,
  // making setDurationAndGrow() a no-op (extra == 0).
  m_appendCmd = std::make_unique<AppendSequenceSection>(*seqModel, sectionDuration);
  m_moveCmd = std::make_unique<Scenario::Command::MoveEventMeta>(
      scenario, m_endEventId, newDate, y, ExpandMode::GrowShrink, LockMode::Free);
}

ExtendSequence::~ExtendSequence() = default;

void ExtendSequence::update(
    const Scenario::ProcessModel& scenario,
    const Id<Scenario::StateModel>&, const TimeVal& newDate, double y)
{
  if(!m_moveCmd)
    return;
  // Only the end date changes frame-to-frame; the section is kept.
  m_moveCmd->update(
      const_cast<Scenario::ProcessModel&>(scenario), m_endEventId, newDate, y,
      ExpandMode::GrowShrink, LockMode::Free);
}

void ExtendSequence::redo(const score::DocumentContext& ctx) const
{
  // On the first call: create the section, then move the parent event.
  // On subsequent calls (after update()): only move the parent event;
  // setDurationAndGrow() in the SequenceModel expands the last section.
  if(!m_sectionApplied)
  {
    m_appendCmd->redo(ctx);
    m_sectionApplied = true;
  }
  m_moveCmd->redo(ctx);
}

void ExtendSequence::undo(const score::DocumentContext& ctx) const
{
  // Reverse: undo the event move first so setDurationAndShrink shrinks
  // the appended section, then remove the section entirely.
  m_moveCmd->undo(ctx);
  if(m_sectionApplied)
  {
    m_appendCmd->undo(ctx);
    m_sectionApplied = false;
  }
}

void ExtendSequence::serializeImpl(DataStreamInput& s) const
{
  s << m_endEventId;
  s << m_appendCmd->serialize();
  s << m_moveCmd->serialize();
  s << m_sectionApplied;
}

void ExtendSequence::deserializeImpl(DataStreamOutput& s)
{
  s >> m_endEventId;

  QByteArray appendData;
  s >> appendData;
  m_appendCmd = std::make_unique<AppendSequenceSection>();
  m_appendCmd->deserialize(appendData);

  QByteArray moveData;
  s >> moveData;
  m_moveCmd = std::make_unique<Scenario::Command::MoveEventMeta>();
  m_moveCmd->deserialize(moveData);

  s >> m_sectionApplied;
}

}
}
