// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddLayerModelToSlot.hpp"

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <vector>

namespace Scenario
{
namespace Command
{

AddLayerModelToSlot::AddLayerModelToSlot(const SlotPath& slot, Id<Process::ProcessModel> process)
    : m_slot{slot}, m_processId{std::move(process)}
{
}

AddLayerModelToSlot::AddLayerModelToSlot(
    const SlotPath& slot,
    const Process::ProcessModel& process)
    : m_slot{slot}, m_processId{process.id()}
{
}

void AddLayerModelToSlot::undo(const score::DocumentContext& ctx) const
{
  auto cst = m_slot.interval.try_find(ctx);
  if (cst)
  {
    cst->removeLayer(m_slot.index, m_processId);
  }
}

void AddLayerModelToSlot::redo(const score::DocumentContext& ctx) const
{
  auto& cst = m_slot.interval.find(ctx);
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
