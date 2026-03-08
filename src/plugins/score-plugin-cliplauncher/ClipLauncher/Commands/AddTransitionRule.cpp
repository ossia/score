#include "AddTransitionRule.hpp"

#include <score/model/path/PathSerialization.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

AddTransitionRule::AddTransitionRule(
    const ProcessModel& proc, const CellModel& cell, TransitionRule rule)
    : m_path{proc}
    , m_cellId{cell.id()}
    , m_rule{std::move(rule)}
{
}

void AddTransitionRule::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).removeTransitionRule(m_rule.id);
}

void AddTransitionRule::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).cells.at(m_cellId).addTransitionRule(m_rule);
}

void AddTransitionRule::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cellId << m_rule;
}

void AddTransitionRule::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_cellId >> m_rule;
}

} // namespace ClipLauncher
