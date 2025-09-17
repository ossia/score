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
}
