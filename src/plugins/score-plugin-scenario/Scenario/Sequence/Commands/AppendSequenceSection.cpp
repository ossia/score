// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AppendSequenceSection.hpp"

#include <Process/TimeValueSerialization.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

namespace Sequence
{
namespace Command
{

AppendSequenceSection::AppendSequenceSection(const SequenceModel& seq, TimeVal duration)
    : m_path{seq}
    , m_duration{duration}
{
  // Pre-allocate IDs at construction time so they remain stable across
  // undo/redo cycles. If we generated them inside redo(), every redo would
  // pick a new max+1 and any Path<> handle held by selections, inspector
  // widgets, or executor components would dangle.
  m_info.prevEndTimeSyncId = seq.endTimeSyncId();
  m_info.prevEndEventId = seq.endEventId();
  m_info.prevDuration = seq.duration();
  m_info.newEndTimeSyncId = getStrongId(seq.timeSyncs);
  m_info.newEndEventId = getStrongId(seq.events);
  m_info.newEndStateId = getStrongId(seq.states);
  m_info.newIntervalId = getStrongId(seq.intervals);
}

void AppendSequenceSection::undo(const score::DocumentContext& ctx) const
{
  auto& seq = m_path.find(ctx);
  seq.undoAppendSection(m_info);
}

void AppendSequenceSection::redo(const score::DocumentContext& ctx) const
{
  auto& seq = m_path.find(ctx);
  seq.appendSectionWithIds(m_info, m_duration);
  m_firstRedo = false;
}

void AppendSequenceSection::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_duration;
  s << m_info.prevEndTimeSyncId << m_info.prevEndEventId;
  s << m_info.newEndTimeSyncId << m_info.newEndEventId;
  s << m_info.newEndStateId << m_info.newIntervalId;
  s << m_info.prevDuration;
}

void AppendSequenceSection::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_duration;
  s >> m_info.prevEndTimeSyncId >> m_info.prevEndEventId;
  s >> m_info.newEndTimeSyncId >> m_info.newEndEventId;
  s >> m_info.newEndStateId >> m_info.newIntervalId;
  s >> m_info.prevDuration;
  m_firstRedo = false;
}

}
}
