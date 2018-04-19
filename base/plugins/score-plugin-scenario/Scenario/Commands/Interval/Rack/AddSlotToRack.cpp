// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddSlotToRack.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
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

AddSlotToRack::AddSlotToRack(const Path<IntervalModel>& rackPath)
    : m_path{rackPath}
{
}

void AddSlotToRack::undo(const score::DocumentContext& ctx) const
{
  auto& rack = m_path.find(ctx);
  rack.removeSlot(rack.smallView().size() - 1);
}

void AddSlotToRack::redo(const score::DocumentContext& ctx) const
{
  auto& rack = m_path.find(ctx);
  auto h = score::AppContext()
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
