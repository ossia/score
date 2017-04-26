#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <algorithm>

#include "PutLayerModelToFront.hpp"
#include <iscore/model/path/Path.hpp>

#include <iscore/model/Identifier.hpp>
namespace Scenario
{
PutLayerModelToFront::PutLayerModelToFront(
    SlotPath&& slotPath, const Id<Process::ProcessModel>& pid)
    : m_slotPath{std::move(slotPath)}, m_pid{pid}
{
  ISCORE_ASSERT(m_slotPath.index == Slot::SmallView);
}

void PutLayerModelToFront::redo() const
{
  m_slotPath.constraint.find().putLayerToFront(m_slotPath.index, m_pid);
}
}
