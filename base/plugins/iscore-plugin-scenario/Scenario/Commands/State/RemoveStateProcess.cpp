#include "RemoveStateProcess.hpp"
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

RemoveStateProcess::RemoveStateProcess(
    const Scenario::StateModel& state,
    Id<Process::StateProcess> processId)
    : m_path{state}, m_processId{std::move(processId)}
{
  auto& p = state.stateProcesses.at(m_processId);
  m_processUuid = p.concreteKey();
}

void RemoveStateProcess::undo(const iscore::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);
  // Create process model
  auto proc = ctx.app.interfaces<Process::StateProcessList>()
                  .get(m_processUuid)
                  ->make(m_processId, &state);

  state.stateProcesses.add(proc);
}

void RemoveStateProcess::redo(const iscore::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);
  state.stateProcesses.remove(m_processId);
}

void RemoveStateProcess::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processUuid << m_processId;
}

void RemoveStateProcess::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processUuid >> m_processId;
}
}
}
