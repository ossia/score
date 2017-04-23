#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <algorithm>

#include "PutLayerModelToFront.hpp"
#include <iscore/model/path/Path.hpp>

#include <iscore/model/Identifier.hpp>
namespace Scenario
{
PutLayerModelToFront::PutLayerModelToFront(
    Path<SlotModel>&& slotPath, const Id<Process::ProcessModel>& pid)
    : m_slotPath{std::move(slotPath)}, m_pid{pid}
{
}

void PutLayerModelToFront::redo() const
{
  m_slotPath.find().putToFront(m_pid);
}
}
