#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CopySlot.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{

CopySlot::CopySlot(
    Path<SlotModel>&& slotToCopy, Path<RackModel>&& targetRackPath)
    : m_slotPath{slotToCopy}, m_targetRackPath{targetRackPath}
{
  auto& rack = m_targetRackPath.find();
  m_newSlotId = getStrongId(rack.slotmodels);
}

void CopySlot::undo() const
{
  auto& targetRack = m_targetRackPath.find();
  targetRack.slotmodels.remove(m_newSlotId);
}

void CopySlot::redo() const
{
  const auto& sourceSlot = m_slotPath.find();
  auto& targetRack = m_targetRackPath.find();

  targetRack.addSlot(new SlotModel{sourceSlot, m_newSlotId, &targetRack});
}

void CopySlot::serializeImpl(DataStreamInput& s) const
{
  s << m_slotPath << m_targetRackPath << m_newSlotId;
}

void CopySlot::deserializeImpl(DataStreamOutput& s)
{
  s >> m_slotPath >> m_targetRackPath >> m_newSlotId;
}
}
}
