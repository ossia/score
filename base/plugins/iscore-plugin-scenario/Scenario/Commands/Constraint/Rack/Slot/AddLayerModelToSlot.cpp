#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <Process/ProcessList.hpp>
#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "AddLayerModelToSlot.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/application/ApplicationContext.hpp>

namespace Scenario
{
namespace Command
{

AddLayerModelToSlot::AddLayerModelToSlot(
  Path<SlotModel>&& slot, Id<Process::ProcessModel> process)
  : m_slot{std::move(slot)}
  , m_processId{std::move(process)}
{

}

AddLayerModelToSlot::AddLayerModelToSlot(
    const SlotModel& slot, const Process::ProcessModel& process)
    : m_slot{slot}
    , m_processId{process.id()}
{
}

void AddLayerModelToSlot::undo() const
{
  auto slot = m_slot.try_find();
  if (slot)
    slot->removeLayer(m_processId);
}

void AddLayerModelToSlot::redo() const
{
  auto& slot = m_slot.find();
  slot.addLayer(m_processId);
}

void AddLayerModelToSlot::serializeImpl(DataStreamInput& s) const
{
  s << m_slot << m_processId;
}

void AddLayerModelToSlot::deserializeImpl(DataStreamOutput& s)
{
  s >> m_slot >> m_processId;
}
}
}
