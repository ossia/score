#include "AddCell.hpp"

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

AddCell::AddCell(const ProcessModel& model, int lane, int scene)
    : m_path{model}
    , m_cellId{getStrongId(model.cells)}
    , m_lane{lane}
    , m_scene{scene}
{
}

void AddCell::undo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  proc.cells.remove(m_cellId);
}

void AddCell::redo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  proc.cells.add(
      ProcessModel::createDefaultCell(m_cellId, m_lane, m_scene, proc.context(), &proc));
}

void AddCell::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cellId << m_lane << m_scene;
}

void AddCell::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_cellId >> m_lane >> m_scene;
}

} // namespace ClipLauncher
