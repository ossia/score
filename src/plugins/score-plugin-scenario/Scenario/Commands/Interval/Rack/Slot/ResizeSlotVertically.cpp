// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ResizeSlotVertically.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
namespace Command
{

ResizeSlotVertically::ResizeSlotVertically(
    const IntervalModel& cst,
    const SlotPath& slotPath,
    double newSize)
    : m_path{slotPath}, m_newSize{newSize}
{
  m_originalSize = cst.getSlotHeight(m_path);
}

ResizeSlotVertically::ResizeSlotVertically(
    const IntervalModel& cst,
    SlotPath&& slotPath,
    double newSize)
    : m_path{slotPath}, m_newSize{newSize}
{
  m_originalSize = cst.getSlotHeight(m_path);
}

void ResizeSlotVertically::undo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.interval.find(ctx);
  cst.setSlotHeight(m_path, m_originalSize);
}

void ResizeSlotVertically::redo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.interval.find(ctx);
  cst.setSlotHeight(m_path, m_newSize);
}

void ResizeSlotVertically::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_originalSize << m_newSize;
}

// Would be better in a ctor ?
void ResizeSlotVertically::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_originalSize >> m_newSize;
}
}
}
