#include <Process/Commands/Properties.hpp>

namespace Process
{
SetDuration::SetDuration(
    const ProcessModel& proc, TimeVal newDuration, ExpandMode mode)
    : m_model{proc}
    , m_old{proc.duration()}
    , m_new{newDuration}
    , m_mode{mode}
{
}

void SetDuration::undo(const score::DocumentContext& ctx) const
{
  if(auto* proc = m_model.try_find(ctx))
    proc->setParentDuration(m_mode, m_old);
}

void SetDuration::redo(const score::DocumentContext& ctx) const
{
  if(auto* proc = m_model.try_find(ctx))
    proc->setParentDuration(m_mode, m_new);
}

void SetDuration::update(unused_t, TimeVal newDuration, ExpandMode mode)
{
  m_new = newDuration;
  m_mode = mode;
}

void SetDuration::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new << (int8_t)m_mode;
}

void SetDuration::deserializeImpl(DataStreamOutput& s)
{
  int8_t mode{};
  s >> m_model >> m_old >> m_new >> mode;
  m_mode = static_cast<ExpandMode>(mode);
}
}
