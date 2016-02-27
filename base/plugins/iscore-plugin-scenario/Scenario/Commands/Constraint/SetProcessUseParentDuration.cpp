#include <Process/Process.hpp>
#include <algorithm>

#include "SetProcessUseParentDuration.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>


namespace Scenario
{
namespace Command
{
SetProcessUseParentDuration::SetProcessUseParentDuration(
        Path<Process::ProcessModel>&& constraintPath,
        bool b) :
    m_path {std::move(constraintPath)},
    m_useParentDuration {b}
{
}

void SetProcessUseParentDuration::undo() const
{
    m_path.find().setUseParentDuration(!m_useParentDuration);
}

void SetProcessUseParentDuration::redo() const
{
    m_path.find().setUseParentDuration(m_useParentDuration);
}

void SetProcessUseParentDuration::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_useParentDuration;
}

void SetProcessUseParentDuration::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_useParentDuration;
}
}
}
