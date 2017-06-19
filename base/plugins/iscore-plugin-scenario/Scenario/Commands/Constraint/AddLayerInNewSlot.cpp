#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>

#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "AddLayerInNewSlot.hpp"
#include <Process/ProcessList.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
AddLayerInNewSlot::AddLayerInNewSlot(
    Path<ConstraintModel>&& constraintPath, Id<Process::ProcessModel> process)
  : m_path{std::move(constraintPath)}
  , m_processId{std::move(process)}
{
}

void AddLayerInNewSlot::undo(const iscore::DocumentContext& ctx) const
{
  auto& constraint = m_path.find(ctx);
  constraint.removeSlot(int(constraint.smallView().size() - 1));
}

void AddLayerInNewSlot::redo(const iscore::DocumentContext& ctx) const
{
  auto& constraint = m_path.find(ctx);
  auto h = context.settings<Scenario::Settings::Model>().getSlotHeight();

  constraint.addSlot(Slot{{m_processId}, m_processId, h});
}

void AddLayerInNewSlot::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processId;
}

void AddLayerInNewSlot::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processId;
}
}
}
