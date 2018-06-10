#include "MoveProcess.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>
#include <ossia/detail/algorithms.hpp>
#include <score/model/path/ObjectPath.hpp>
namespace Scenario::Command
{

MoveProcess::MoveProcess(
    const IntervalModel& src
    , const IntervalModel& tgt
    , Id<Process::ProcessModel> processId):
  m_src{src}
, m_tgt{tgt}
, m_processId{processId}
, m_oldPos{std::distance(src.fullView().begin(), ossia::find(src.fullView(), FullSlot{processId}))}
{

}

void MoveProcess::undo(const score::DocumentContext& ctx) const
{
  auto& src = m_tgt.find(ctx);
  auto& tgt = m_src.find(ctx);

  auto& proc = src.processes.at(m_processId);
  src.processes.erase(proc);
  tgt.processes.add(&proc);
}

void MoveProcess::redo(const score::DocumentContext& ctx) const
{
  auto& src = m_src.find(ctx);
  auto& tgt = m_tgt.find(ctx);

  auto& proc = src.processes.at(m_processId);
  tgt.processes.add(&proc);

  Dataflow::reparentCables(&proc, m_src.unsafePath(), m_tgt.unsafePath(), ctx);

  src.processes.erase(proc);
}

void MoveProcess::serializeImpl(DataStreamInput& s) const
{
  s << m_src << m_tgt << m_processId << m_oldPos;
}

void MoveProcess::deserializeImpl(DataStreamOutput& s)
{
  s >> m_src >> m_tgt >> m_processId >> m_oldPos;
}
}
