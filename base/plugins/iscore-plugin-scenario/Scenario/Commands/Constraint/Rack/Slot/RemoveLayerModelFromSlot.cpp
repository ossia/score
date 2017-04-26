
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/model/path/RelativePath.hpp>

#include "RemoveLayerModelFromSlot.hpp"
#include <Process/ProcessList.hpp>
#include <Process/Process.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/application/ApplicationContext.hpp>

namespace Scenario
{
namespace Command
{

RemoveLayerModelFromSlot::RemoveLayerModelFromSlot(
    SlotPath&& rackPath, Id<Process::ProcessModel> layerId)
    : m_path{std::move(rackPath)}, m_layerId{std::move(layerId)}
{
}

void RemoveLayerModelFromSlot::undo() const
{
  auto& slot = m_path.constraint.find();
  slot.addLayer(m_path.index, m_layerId);
}

void RemoveLayerModelFromSlot::redo() const
{
  auto& slot = m_path.constraint.find();
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
