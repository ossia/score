#include "AddStateProcess.hpp"
#include <Process/StateProcess.hpp>
#include <Process/StateProcessFactory.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
namespace Scenario
{
namespace Command
{

AddStateProcessToState::AddStateProcessToState(
    const Scenario::StateModel& state,
    UuidKey<Process::StateProcessFactory> process)
    : AddStateProcessToState{state,
                             getStrongId(state.stateProcesses), process}
{
}

AddStateProcessToState::AddStateProcessToState(
    const Scenario::StateModel& state,
    Id<Process::StateProcess>
        processId,
    UuidKey<Process::StateProcessFactory>
        process)
    : m_path{state}
    , m_processName{process}
    , m_createdProcessId{std::move(processId)}
{
}

void AddStateProcessToState::undo(const iscore::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);
  state.stateProcesses.remove(m_createdProcessId);
}

void AddStateProcessToState::redo(const iscore::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);
  // Create process model
  auto proc = ctx.app.interfaces<Process::StateProcessList>()
                  .get(m_processName)
                  ->make(m_createdProcessId, &state);

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
