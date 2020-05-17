// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetProcessPosition.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
PutProcessBefore::PutProcessBefore(
    const IntervalModel& cst,
    optional<Id<Process::ProcessModel>> proc,
    Id<Process::ProcessModel> proc2)
    : m_path{cst}, m_proc{std::move(proc)}, m_proc2{std::move(proc2)}
{
  auto& id_map = cst.processes.map();
  auto& hash = id_map.m_map;
  auto& seq = id_map.m_order;

  // 1. Find elements
  auto it2_hash = hash.find(proc2);
  SCORE_ASSERT(it2_hash != hash.end());

  std::list<Process::ProcessModel*>::const_iterator it2_order = it2_hash.value().second;
  auto next = it2_order++;
  if (next != seq.end())
  {
    m_old_after_proc2 = (*next)->id();
  }
  else
  {
    m_old_after_proc2 = {};
  }
}

void PutProcessBefore::undo(const score::DocumentContext& ctx) const
{
  putBefore(ctx, m_old_after_proc2, m_proc2);
}

void PutProcessBefore::redo(const score::DocumentContext& ctx) const
{
  putBefore(ctx, m_proc, m_proc2);
}

void PutProcessBefore::putBefore(
    const score::DocumentContext& ctx,
    optional<Id<Process::ProcessModel>> t1,
    Id<Process::ProcessModel> t2) const
{
  auto& cst = m_path.find(ctx);

  auto& id_map = cst.processes.unsafe_map();
  auto& hash = id_map.m_map;
  auto& seq = id_map.m_order;

  if (t1 == t2)
    return;

  // 1. Find elements
  auto it2_hash = hash.find(t2);
  SCORE_ASSERT(it2_hash != hash.end());
  auto& it2_order = it2_hash.value().second;

  if (t1)
  {
    // put before t1
    auto it1_hash = hash.find(*t1);
    SCORE_ASSERT(it1_hash != hash.end());
    auto& it1_order = it1_hash.value().second;

    auto new_it2 = seq.insert(it1_order, *it2_order);
    seq.erase(it2_order);
    it2_order = new_it2;
  }
  else
  {
    // put at end
    auto new_it2 = seq.insert(seq.end(), *it2_order);
    seq.erase(it2_order);
    it2_order = new_it2;
  }

  cst.processes.orderChanged();
}

void PutProcessBefore::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_proc << m_proc2 << m_old_after_proc2;
}

void PutProcessBefore::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_proc >> m_proc2 >> m_old_after_proc2;
}

PutStateProcessBefore::PutStateProcessBefore(
    const StateModel& cst,
    optional<Id<Process::ProcessModel>> proc,
    Id<Process::ProcessModel> proc2)
    : m_path{cst}, m_proc{std::move(proc)}, m_proc2{std::move(proc2)}
{
  auto& id_map = cst.stateProcesses.map();
  auto& hash = id_map.m_map;
  auto& seq = id_map.m_order;

  // 1. Find elements
  auto it2_hash = hash.find(proc2);
  SCORE_ASSERT(it2_hash != hash.end());

  std::list<Process::ProcessModel*>::const_iterator it2_order = it2_hash.value().second;
  auto next = it2_order++;
  if (next != seq.end())
  {
    m_old_after_proc2 = (*next)->id();
  }
  else
  {
    m_old_after_proc2 = {};
  }
}

void PutStateProcessBefore::undo(const score::DocumentContext& ctx) const
{
  putBefore(ctx, m_old_after_proc2, m_proc2);
}

void PutStateProcessBefore::redo(const score::DocumentContext& ctx) const
{
  putBefore(ctx, m_proc, m_proc2);
}

void PutStateProcessBefore::putBefore(
    const score::DocumentContext& ctx,
    optional<Id<Process::ProcessModel>> t1,
    Id<Process::ProcessModel> t2) const
{
  auto& cst = m_path.find(ctx);

  auto& id_map = cst.stateProcesses.unsafe_map();
  auto& hash = id_map.m_map;
  auto& seq = id_map.m_order;

  if (t1 == t2)
    return;

  // 1. Find elements
  auto it2_hash = hash.find(t2);
  SCORE_ASSERT(it2_hash != hash.end());
  auto& it2_order = it2_hash.value().second;

  if (t1)
  {
    // put before t1
    auto it1_hash = hash.find(*t1);
    SCORE_ASSERT(it1_hash != hash.end());
    auto& it1_order = it1_hash.value().second;

    auto new_it2 = seq.insert(it1_order, *it2_order);
    seq.erase(it2_order);
    it2_order = new_it2;
  }
  else
  {
    // put at end
    auto new_it2 = seq.insert(seq.end(), *it2_order);
    seq.erase(it2_order);
    it2_order = new_it2;
  }

  cst.stateProcesses.orderChanged();
}

void PutStateProcessBefore::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_proc << m_proc2 << m_old_after_proc2;
}

void PutStateProcessBefore::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_proc >> m_proc2 >> m_old_after_proc2;
}
}
}
