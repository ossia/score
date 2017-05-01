#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "AddSlotToRack.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/application/ApplicationContext.hpp>

namespace Scenario
{
namespace Command
{

AddSlotToRack::AddSlotToRack(const Path<ConstraintModel>&  rackPath)
  : m_path{rackPath}
{
}

void AddSlotToRack::undo() const
{
  auto& rack = m_path.find();
  rack.removeSlot(rack.smallView().size() - 1);
}

void AddSlotToRack::redo() const
{
  auto& rack = m_path.find();
  auto h = iscore::AppContext()
               .settings<Scenario::Settings::Model>()
               .getSlotHeight();

  rack.addSlot(Slot{{}, Id<Process::ProcessModel>{}, h});
}

void AddSlotToRack::serializeImpl(DataStreamInput& s) const
{
  s << m_path;
}

void AddSlotToRack::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path;
}
}
}
