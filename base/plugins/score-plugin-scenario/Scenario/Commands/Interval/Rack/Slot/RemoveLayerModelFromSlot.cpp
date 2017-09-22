// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/model/path/RelativePath.hpp>

#include "RemoveLayerModelFromSlot.hpp"
#include <Process/ProcessList.hpp>
#include <Process/Process.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/application/ApplicationContext.hpp>

namespace Scenario
{
namespace Command
{

RemoveLayerModelFromSlot::RemoveLayerModelFromSlot(
    SlotPath&& rackPath, Id<Process::ProcessModel> layerId)
    : m_path{std::move(rackPath)}, m_layerId{std::move(layerId)}
{
}

void RemoveLayerModelFromSlot::undo(const score::DocumentContext& ctx) const
{
  auto& slot = m_path.interval.find(ctx);
  slot.addLayer(m_path.index, m_layerId);
}

void RemoveLayerModelFromSlot::redo(const score::DocumentContext& ctx) const
{
  auto& slot = m_path.interval.find(ctx);
  slot.removeLayer(m_path.index, m_layerId);
}

void RemoveLayerModelFromSlot::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_layerId;
}

void RemoveLayerModelFromSlot::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_layerId;
}
}
}
