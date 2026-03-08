#include "SetCellProperties.hpp"

#include <score/model/path/PathSerialization.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

// --- SetCellLaunchMode ---

SetCellLaunchMode::SetCellLaunchMode(
    const ProcessModel& proc, const CellModel& cell, LaunchMode newMode)
    : m_path{proc}
    , m_cellId{cell.id()}
    , m_old{cell.launchMode()}
    , m_new{newMode}
{
}

void SetCellLaunchMode::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).setLaunchMode(m_old);
}

void SetCellLaunchMode::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).setLaunchMode(m_new);
}

void SetCellLaunchMode::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cellId << m_old << m_new;
}

void SetCellLaunchMode::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_cellId >> m_old >> m_new;
}

// --- SetCellTriggerStyle ---

SetCellTriggerStyle::SetCellTriggerStyle(
    const ProcessModel& proc, const CellModel& cell, TriggerStyle newStyle)
    : m_path{proc}
    , m_cellId{cell.id()}
    , m_old{cell.triggerStyle()}
    , m_new{newStyle}
{
}

void SetCellTriggerStyle::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).setTriggerStyle(m_old);
}

void SetCellTriggerStyle::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).setTriggerStyle(m_new);
}

void SetCellTriggerStyle::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cellId << m_old << m_new;
}

void SetCellTriggerStyle::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_cellId >> m_old >> m_new;
}

// --- SetCellVelocity ---

SetCellVelocity::SetCellVelocity(
    const ProcessModel& proc, const CellModel& cell, double newVel)
    : m_path{proc}
    , m_cellId{cell.id()}
    , m_old{cell.velocity()}
    , m_new{newVel}
{
}

void SetCellVelocity::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).setVelocity(m_old);
}

void SetCellVelocity::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).setVelocity(m_new);
}

void SetCellVelocity::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cellId << m_old << m_new;
}

void SetCellVelocity::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_cellId >> m_old >> m_new;
}

} // namespace ClipLauncher
