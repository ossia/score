// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddStateProcess.hpp"
#include <Process/StateProcess.hpp>
#include <Process/StateProcessFactory.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
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

void AddStateProcessToState::undo(const score::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);
  state.stateProcesses.remove(m_createdProcessId);
}

void AddStateProcessToState::redo(const score::DocumentContext& ctx) const
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
