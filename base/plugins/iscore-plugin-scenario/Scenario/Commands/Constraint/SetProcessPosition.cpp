#include "SetProcessPosition.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Scenario
{
namespace Command
{
SwapProcessPosition::SwapProcessPosition(
        Scenario::ConstraintModel& cst,
        const Id<Process::ProcessModel>& proc,
        const Id<Process::ProcessModel>& proc2) :
    m_path{cst},
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
