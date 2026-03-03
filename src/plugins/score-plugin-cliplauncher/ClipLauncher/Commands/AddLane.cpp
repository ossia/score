#include "AddLane.hpp"

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/LaneModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

AddLane::AddLane(const ProcessModel& model, int position)
    : m_path{model}
    , m_laneId{getStrongId(model.lanes)}
    , m_position{position}
{
  // Pre-generate cell IDs for each scene
  m_cellIds = getStrongIdRange<CellModel>(model.sceneCount(), model.cells);
}

void AddLane::undo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  // Remove cells first
  for(auto& cellId : m_cellIds)
    proc.cells.remove(cellId);
  proc.lanes.remove(m_laneId);
}

void AddLane::redo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  auto lane = new LaneModel{m_laneId, &proc};
  lane->setName(QString("Lane %1").arg(m_position + 1));
  proc.lanes.add(lane);

  // Auto-create cells for every scene
  int sceneIdx = 0;
  for(auto& cellId : m_cellIds)
  {
    proc.cells.add(ProcessModel::createDefaultCell(
        cellId, m_position, sceneIdx, proc.context(), &proc));
    sceneIdx++;
  }
}

void AddLane::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_laneId << m_cellIds << m_position;
}

void AddLane::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_laneId >> m_cellIds >> m_position;
}

} // namespace ClipLauncher
