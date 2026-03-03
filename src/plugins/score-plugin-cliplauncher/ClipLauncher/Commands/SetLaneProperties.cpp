#include "SetLaneProperties.hpp"

#include <score/model/path/PathSerialization.hpp>

#include <ClipLauncher/LaneModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

// --- SetLaneName ---

SetLaneName::SetLaneName(
    const ProcessModel& proc, const LaneModel& lane, QString newName)
    : m_path{proc}
    , m_laneId{lane.id()}
    , m_old{lane.name()}
    , m_new{std::move(newName)}
{
}

void SetLaneName::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).lanes.at(m_laneId).setName(m_old);
}

void SetLaneName::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).lanes.at(m_laneId).setName(m_new);
}

void SetLaneName::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_laneId << m_old << m_new;
}

void SetLaneName::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_laneId >> m_old >> m_new;
}

// --- SetLaneExclusivityMode ---

SetLaneExclusivityMode::SetLaneExclusivityMode(
    const ProcessModel& proc, const LaneModel& lane, ExclusivityMode newMode)
    : m_path{proc}
    , m_laneId{lane.id()}
    , m_old{lane.exclusivityMode()}
    , m_new{newMode}
{
}

void SetLaneExclusivityMode::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).lanes.at(m_laneId).setExclusivityMode(m_old);
}

void SetLaneExclusivityMode::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).lanes.at(m_laneId).setExclusivityMode(m_new);
}

void SetLaneExclusivityMode::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_laneId << m_old << m_new;
}

void SetLaneExclusivityMode::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_laneId >> m_old >> m_new;
}

} // namespace ClipLauncher
