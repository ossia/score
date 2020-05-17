// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetAutoTrigger.hpp"

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{

SetAutoTrigger::SetAutoTrigger(const TimeSyncModel& tn, bool b)
    : m_path{std::move(tn)}, m_old{tn.autotrigger()}, m_new{b}
{
}

void SetAutoTrigger::undo(const score::DocumentContext& ctx) const
{
  auto& tn = m_path.find(ctx);
  tn.setAutotrigger(m_old);
}

void SetAutoTrigger::redo(const score::DocumentContext& ctx) const
{
  auto& tn = m_path.find(ctx);
  tn.setAutotrigger(m_new);
}

void SetAutoTrigger::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old << m_new;
}

void SetAutoTrigger::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old >> m_new;
}
}
}
