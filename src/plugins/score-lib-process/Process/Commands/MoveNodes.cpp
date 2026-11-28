#include <Process/Commands/Properties.hpp>

namespace Process
{
MoveNodes::MoveNodes(std::vector<const ProcessModel*> processes, QPointF delta)
    : m_delta{delta}
{
  for(auto& proc : processes)
  {
    m_models.emplace_back(Path{*proc}, proc->position());
  }
}

void MoveNodes::undo(const score::DocumentContext& ctx) const
{
  for(auto& [path, orig_pos] : m_models)
  {
    if(auto* proc = path.try_find(ctx))
    {
      proc->setPosition(orig_pos);
    }
    else
    {
      qDebug() << "Warning! trying to move a destroyed node";
    }
  }
}

void MoveNodes::redo(const score::DocumentContext& ctx) const
{
  for(auto& [path, orig_pos] : m_models)
  {
    if(auto* proc = path.try_find(ctx))
    {
      proc->setPosition(orig_pos + m_delta);
    }
    else
    {
      qDebug() << "Warning! trying to move a destroyed node";
    }
  }
}

void MoveNodes::update(unused_t, QPointF delta)
{
  m_delta = delta;
}

void MoveNodes::serializeImpl(DataStreamInput& s) const
{
  s << m_models << m_delta;
}

void MoveNodes::deserializeImpl(DataStreamOutput& s)
{
  s >> m_models >> m_delta;
}

SetNodePositions::SetNodePositions(
    const std::vector<std::pair<const ProcessModel*, QPointF>>& positions)
{
  for(const auto& [proc, pos] : positions)
  {
    if(proc->position() == pos)
      continue;
    m_before.emplace_back(Path{*proc}, proc->position());
    m_after.push_back(pos);
  }
}

bool SetNodePositions::movesAnything() const noexcept
{
  return !m_after.empty();
}

void SetNodePositions::undo(const score::DocumentContext& ctx) const
{
  for(const auto& [path, pos] : m_before)
    if(auto* proc = path.try_find(ctx))
      proc->setPosition(pos);
}

void SetNodePositions::redo(const score::DocumentContext& ctx) const
{
  for(std::size_t i = 0; i < m_before.size(); i++)
    if(auto* proc = m_before[i].first.try_find(ctx))
      proc->setPosition(m_after[i]);
}

void SetNodePositions::serializeImpl(DataStreamInput& s) const
{
  s << m_before << m_after;
}

void SetNodePositions::deserializeImpl(DataStreamOutput& s)
{
  s >> m_before >> m_after;
  if(m_after.size() != m_before.size())
  {
    m_before.clear();
    m_after.clear();
  }
}
}
