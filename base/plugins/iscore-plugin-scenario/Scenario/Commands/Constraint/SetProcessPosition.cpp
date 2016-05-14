#include "SetProcessPosition.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Scenario
{
namespace Command
{
SetProcessPosition::SetProcessPosition(
        Path<Scenario::ConstraintModel>&& cst,
        const Id<Process::ProcessModel>& proc,
        const Id<Process::ProcessModel>& proc2) :
    m_path{std::move(cst)},
    m_proc{proc},
    m_proc2{proc2}
{

}


void SetProcessPosition::undo() const
{
    redo();
}

void SetProcessPosition::redo() const
{
    auto& cst = m_path.find();
    cst.processes.relocate(m_proc, m_proc2);
}

void SetProcessPosition::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_proc << m_proc2;
}

void SetProcessPosition::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_proc >> m_proc2;
}


SwapProcessPosition::SwapProcessPosition(
        Path<Scenario::ConstraintModel>&& cst,
        const Id<Process::ProcessModel>& proc,
        const Id<Process::ProcessModel>& proc2) :
    m_path{std::move(cst)},
    m_proc{proc},
    m_proc2{proc2}
{

}


void SwapProcessPosition::undo() const
{
    redo();
}

void SwapProcessPosition::redo() const
{
    auto& cst = m_path.find();
    cst.processes.swap(m_proc, m_proc2);
}

void SwapProcessPosition::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_proc << m_proc2;
}

void SwapProcessPosition::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_proc >> m_proc2;
}
}
}
