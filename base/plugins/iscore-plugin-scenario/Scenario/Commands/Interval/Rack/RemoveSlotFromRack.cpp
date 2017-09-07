// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <type_traits>
#include <utility>

#include "RemoveSlotFromRack.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/path/ObjectIdentifier.hpp>

namespace Scenario
{
namespace Command
{
RemoveSlotFromRack::RemoveSlotFromRack(
    SlotPath slotPath,
    Slot slt):
  m_path{std::move(slotPath)}
, m_slot{std::move(slt)}
{
}

void RemoveSlotFromRack::undo(const iscore::DocumentContext& ctx) const
{
  auto& rack = m_path.interval.find(ctx);
  rack.addSlot(m_slot, m_path.index);
}

void RemoveSlotFromRack::redo(const iscore::DocumentContext& ctx) const
{
  auto& rack = m_path.interval.find(ctx);
  rack.removeSlot(m_path.index);
}

void RemoveSlotFromRack::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_slot;
}

void RemoveSlotFromRack::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_slot;
}
}
}
