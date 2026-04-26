// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MoveSequenceIS.hpp"

#include <Process/TimeValueSerialization.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

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

}
}
