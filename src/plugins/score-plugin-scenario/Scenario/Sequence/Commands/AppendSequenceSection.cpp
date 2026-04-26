// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AppendSequenceSection.hpp"

#include <Process/TimeValueSerialization.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Sequence
{
namespace Command
{

AppendSequenceSection::AppendSequenceSection(const SequenceModel& seq, TimeVal duration)
    : m_path{seq}
    , m_duration{duration}
{
}

void AppendSequenceSection::undo(const score::DocumentContext& ctx) const
{
  auto& seq = m_path.find(ctx);
  seq.undoAppendSection(m_info);
}

void AppendSequenceSection::redo(const score::DocumentContext& ctx) const
{
  auto& seq = m_path.find(ctx);
  m_info = seq.appendSection(m_duration);
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
