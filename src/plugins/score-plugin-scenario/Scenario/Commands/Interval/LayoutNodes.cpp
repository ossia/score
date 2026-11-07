#include "LayoutNodes.hpp"

#include <Process/Process.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario::Command
{
LayoutNodes::LayoutNodes(const std::vector<NodePlacement>& placements)
{
  for(const auto& p : placements)
  {
    m_before.emplace_back(Path{*p.process}, p.process->position());
    m_after.push_back(p.position);
  }
}

void LayoutNodes::undo(const score::DocumentContext& ctx) const
{
  for(const auto& [path, pos] : m_before)
    if(auto* proc = path.try_find(ctx))
      proc->setPosition(pos);
}

void LayoutNodes::redo(const score::DocumentContext& ctx) const
{
  for(std::size_t i = 0; i < m_before.size(); i++)
    if(auto* proc = m_before[i].first.try_find(ctx))
      proc->setPosition(m_after[i]);
}

void LayoutNodes::serializeImpl(DataStreamInput& s) const
{
  s << m_before << m_after;
}

void LayoutNodes::deserializeImpl(DataStreamOutput& s)
{
  s >> m_before >> m_after;
}
}
