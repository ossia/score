// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <algorithm>

#include "PutLayerModelToFront.hpp"
#include <score/model/path/Path.hpp>

#include <score/model/Identifier.hpp>
namespace Scenario
{
PutLayerModelToFront::PutLayerModelToFront(
    SlotPath&& slotPath, const Id<Process::ProcessModel>& pid)
    : m_slotPath{std::move(slotPath)}, m_pid{pid}
{
  // FIXME this assert crashes if there is only one process in the slot
  // SCORE_ASSERT(m_slotPath.index == Slot::SmallView);
}

void PutLayerModelToFront::redo(const score::DocumentContext& ctx) const
{
  m_slotPath.interval.find(ctx).putLayerToFront(m_slotPath.index, m_pid);
}
}
