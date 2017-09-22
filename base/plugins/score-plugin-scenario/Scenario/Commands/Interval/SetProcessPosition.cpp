// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetProcessPosition.hpp"
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
PutProcessBefore::PutProcessBefore(
    const IntervalModel& cst,
    Id<Process::ProcessModel>
        proc,
    Id<Process::ProcessModel>
        proc2)
    : m_path{cst}
    , m_proc{std::move(proc)}
    , m_proc2{std::move(proc2)}
{
}

void PutProcessBefore::undo(const score::DocumentContext& ctx) const
{
  redo(ctx);
}

void PutProcessBefore::redo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.find(ctx);
  cst.processes.relocate(m_proc, m_proc2);
}

void PutProcessBefore::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_proc << m_proc2;
}

void PutProcessBefore::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_proc >> m_proc2;
}

PutProcessToEnd::PutProcessToEnd(
    const IntervalModel& cst,
    Id<Process::ProcessModel> proc)
    : m_path{std::move(cst)}, m_proc{std::move(proc)}
{
  auto it = cst.processes.find(m_proc);
  SCORE_ASSERT(it != cst.processes.end());
  std::advance(it, 1);
  SCORE_ASSERT(it != cst.processes.end());
  m_proc_after = it->id();
}

void PutProcessToEnd::undo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.find(ctx);
  cst.processes.relocate(m_proc, m_proc_after);
}

void PutProcessToEnd::redo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.find(ctx);
  cst.processes.putToEnd(m_proc);
}

void PutProcessToEnd::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_proc << m_proc_after;
}

void PutProcessToEnd::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_proc >> m_proc_after;
}

SwapProcessPosition::SwapProcessPosition(
    const IntervalModel& cst,
    Id<Process::ProcessModel>
        proc,
    Id<Process::ProcessModel>
        proc2)
    : m_path{std::move(cst)}
    , m_proc{std::move(proc)}
    , m_proc2{std::move(proc2)}
{
}

void SwapProcessPosition::undo(const score::DocumentContext& ctx) const
{
  redo(ctx);
}

void SwapProcessPosition::redo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.find(ctx);
  cst.processes.swap(m_proc, m_proc2);
}

void SwapProcessPosition::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_proc << m_proc2;
}

void SwapProcessPosition::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_proc >> m_proc2;
}
}
}
