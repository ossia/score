#include <Scenario/Document/Constraint/ConstraintModel.hpp>

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
RemoveSlotFromRack::RemoveSlotFromRack(SlotPath slotPath):
  m_path{slotPath}
, m_slot{slotPath.find()}
{
}

void RemoveSlotFromRack::undo() const
{
  auto& rack = m_path.constraint.find();
  rack.addSlot(m_slot, m_path);
}

void RemoveSlotFromRack::redo() const
{
  auto& rack = m_path.constraint.find();
  rack.removeSlot(m_path);
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
