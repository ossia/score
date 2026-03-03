#include "RemoveTransitionRule.hpp"

#include <score/model/path/PathSerialization.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

RemoveTransitionRule::RemoveTransitionRule(
    const ProcessModel& proc, const CellModel& cell, TransitionRule rule)
    : m_path{proc}
    , m_cellId{cell.id()}
    , m_savedRule{std::move(rule)}
{
}

void RemoveTransitionRule::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).addTransitionRule(m_savedRule);
}

void RemoveTransitionRule::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).removeTransitionRule(m_savedRule.id);
}

void RemoveTransitionRule::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cellId << m_savedRule;
}

void RemoveTransitionRule::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_cellId >> m_savedRule;
}

} // namespace ClipLauncher
