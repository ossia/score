#include "AddStateProcess.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <Process/StateProcess.hpp>
#include <Process/StateProcessFactory.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
namespace Scenario
{
namespace Command
{

AddStateProcessToState::AddStateProcessToState(Path<StateModel>&& state,
        UuidKey<Process::StateProcessFactory> process) :
    AddStateProcessToState{
        std::move(state),
        getStrongId(state.find().stateProcesses),
        process}
{

}

AddStateProcessToState::AddStateProcessToState(Path<StateModel>&& state,
        Id<Process::StateProcess> processId,
        UuidKey<Process::StateProcessFactory> process):
    m_path{std::move(state)},
    m_processName{process},
    m_createdProcessId{std::move(processId)}
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
