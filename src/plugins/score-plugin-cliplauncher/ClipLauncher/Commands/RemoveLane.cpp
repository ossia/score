#include "RemoveLane.hpp"

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/LaneModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

RemoveLane::RemoveLane(const ProcessModel& model, const LaneModel& lane)
    : m_path{model}
    , m_laneId{lane.id()}
    , m_laneData{score::marshall<DataStream>(lane)}
{
  // Also serialize all cells in this lane for undo
  for(auto& cell : model.cells)
  {
    if(cell.lane() == lane.id().val())
    {
      m_cellIds.push_back(cell.id());
      m_cellData.push_back(score::marshall<DataStream>(cell));
    }
  }
}

void RemoveLane::undo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);

  // Restore lane
  DataStream::Deserializer s{m_laneData};
  auto lane = new LaneModel{s, &proc};
  proc.lanes.add(lane);

  // Restore cells
  for(const auto& data : m_cellData)
  {
    DataStream::Deserializer cs{data};
    auto cell = new CellModel{cs, proc.context(), &proc};
    proc.cells.add(cell);
  }
}

void RemoveLane::redo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);

  // Remove cells first
  for(const auto& cellId : m_cellIds)
    proc.cells.remove(cellId);

  proc.lanes.remove(m_laneId);
}

void RemoveLane::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_laneId << m_laneData << m_cellData << m_cellIds;
}

void RemoveLane::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_laneId >> m_laneData >> m_cellData >> m_cellIds;
}

} // namespace ClipLauncher
