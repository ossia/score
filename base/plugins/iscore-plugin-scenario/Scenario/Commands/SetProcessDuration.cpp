#include "SetProcessDuration.hpp"
#include <Process/Process.hpp>
SetProcessDuration::SetProcessDuration(Path<Process>&& path, const TimeValue& newVal) :
    m_path {std::move(path)},
    m_new {newVal}
{
    m_old = m_path.find().duration();
}

void SetProcessDuration::undo() const
{
    m_path.find().setDuration(m_old);
}

void SetProcessDuration::redo() const
{
    m_path.find().setDuration(m_new);
}

void SetProcessDuration::serializeImpl(QDataStream& s) const
{
    s << m_path << m_old << m_new;
}

void SetProcessDuration::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_old >> m_new;
}
