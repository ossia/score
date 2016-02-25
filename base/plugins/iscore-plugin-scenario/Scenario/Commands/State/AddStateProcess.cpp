#include "AddStateProcess.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <Process/StateProcess.hpp>
#include <Process/StateProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
namespace Scenario
{
namespace Command
{

AddStateProcessToState::AddStateProcessToState(
        Path<StateModel>&& statePath,
        const UuidKey<Process::StateProcessFactory>& process) :
    AddStateProcessToState{
        std::move(statePath),
        getStrongId(statePath.find().stateProcesses),
        process}
{

}

AddStateProcessToState::AddStateProcessToState(
        Path<StateModel>&& statePath,
        const Id<Process::StateProcess>& processId,
        const UuidKey<Process::StateProcessFactory>& process):
    m_path{std::move(statePath)},
    m_processName{process},
    m_createdProcessId{processId}
{
}

void AddStateProcessToState::undo() const
{
    auto& state = m_path.find();
    state.stateProcesses.remove(m_createdProcessId);
}

void AddStateProcessToState::redo() const
{
    auto& state = m_path.find();
    // Create process model
    auto proc = context.components
            .factory<Process::StateProcessList>()
            .get(m_processName)
            ->make(
                m_createdProcessId,
                &state);

    state.stateProcesses.add(proc);
}

void AddStateProcessToState::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_processName << m_createdProcessId;
}

void AddStateProcessToState::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_processName >> m_createdProcessId;
}
}
}
