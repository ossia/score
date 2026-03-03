#include "MoveCell.hpp"

#include <score/model/path/PathSerialization.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

MoveCell::MoveCell(
    const ProcessModel& model, const CellModel& cell, int newLane, int newScene)
    : m_path{model}
    , m_cellId{cell.id()}
    , m_oldLane{cell.lane()}
    , m_oldScene{cell.scene()}
    , m_newLane{newLane}
    , m_newScene{newScene}
{
}

void MoveCell::undo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  auto& cell = proc.cells.at(m_cellId);
  cell.setLane(m_oldLane);
  cell.setScene(m_oldScene);
}

void MoveCell::redo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  auto& cell = proc.cells.at(m_cellId);
  cell.setLane(m_newLane);
  cell.setScene(m_newScene);
}

void MoveCell::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cellId << m_oldLane << m_oldScene << m_newLane << m_newScene;
}

void MoveCell::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_cellId >> m_oldLane >> m_oldScene >> m_newLane >> m_newScene;
}

} // namespace ClipLauncher
