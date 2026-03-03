#include "RemoveCell.hpp"

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

RemoveCell::RemoveCell(const ProcessModel& model, const CellModel& cell)
    : m_path{model}
    , m_cellId{cell.id()}
    , m_cellData{score::marshall<DataStream>(cell)}
{
}

void RemoveCell::undo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  DataStream::Deserializer s{m_cellData};
  auto cell = new CellModel{s, proc.context(), &proc};
  proc.cells.add(cell);
}

void RemoveCell::redo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  proc.cells.remove(m_cellId);
}

void RemoveCell::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cellId << m_cellData;
}

void RemoveCell::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_cellId >> m_cellData;
}

} // namespace ClipLauncher
