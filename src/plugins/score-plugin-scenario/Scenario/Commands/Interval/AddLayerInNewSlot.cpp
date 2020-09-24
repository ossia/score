// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddLayerInNewSlot.hpp"

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
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
AddLayerInNewSlot::AddLayerInNewSlot(
    Path<IntervalModel>&& intervalPath,
    Id<Process::ProcessModel> process)
    : m_path{std::move(intervalPath)}, m_processId{std::move(process)}
{
}

void AddLayerInNewSlot::undo(const score::DocumentContext& ctx) const
{
  auto& interval = m_path.find(ctx);
  interval.removeSlot(int(interval.smallView().size() - 1));
}

void AddLayerInNewSlot::redo(const score::DocumentContext& ctx) const
{
  auto& interval = m_path.find(ctx);
  const double h = Scenario::getNewLayerHeight(ctx.app, interval.processes.at(m_processId));

  interval.addSlot(Slot{{m_processId}, m_processId, h});
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
