// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>

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
    const SlotPath& slot,
    Id<Process::ProcessModel> process)
  : m_slot{slot}
  , m_processId{std::move(process)}
{

}

AddLayerModelToSlot::AddLayerModelToSlot(
    const SlotPath& slot, const Process::ProcessModel& process)
    : m_slot{slot}
    , m_processId{process.id()}
{
}

void AddLayerModelToSlot::undo(const iscore::DocumentContext& ctx) const
{
  auto cst = m_slot.constraint.try_find(ctx);
  if (cst)
  {
    cst->removeLayer(m_slot.index, m_processId);
  }
}

void AddLayerModelToSlot::redo(const iscore::DocumentContext& ctx) const
{
  auto& cst = m_slot.constraint.find(ctx);
  cst.addLayer(m_slot.index, m_processId);
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
