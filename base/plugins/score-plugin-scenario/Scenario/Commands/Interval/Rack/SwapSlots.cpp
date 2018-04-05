// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <algorithm>

#include "SwapSlots.hpp"
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
SwapSlots::SwapSlots(
    Path<IntervalModel>&& rack, Slot::RackView v, int first, int second)
    : m_path{std::move(rack)}
    , m_view{v}
    , m_first{std::move(first)}
    , m_second{std::move(second)}
{
}

void SwapSlots::undo(const score::DocumentContext& ctx) const
{
  redo(ctx);
}

void SwapSlots::redo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.find(ctx);
  cst.swapSlots(m_first, m_second, m_view);
}

void SwapSlots::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_view << m_first << m_second;
}

void SwapSlots::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_view >> m_first >> m_second;
}
}
}
