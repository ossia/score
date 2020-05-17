// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RemoveStateProcess.hpp"

#include <Process/ProcessList.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/tools/IdentifierGeneration.hpp>
namespace Scenario
{
namespace Command
{

RemoveStateProcess::RemoveStateProcess(
    const Scenario::StateModel& state,
    Id<Process::ProcessModel> processId)
    : m_path{state}, m_processId{std::move(processId)}
{
  auto& p = state.stateProcesses.at(m_processId);
  m_processUuid = p.concreteKey();
  m_data = score::marshall<DataStream>(p);
}

void RemoveStateProcess::undo(const score::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);
  // Create process model
  DataStream::Deserializer s{m_data};
  auto& fact = ctx.app.interfaces<Process::ProcessFactoryList>();

  auto proc = deserialize_interface(fact, s, ctx, &state);
  SCORE_ASSERT(proc);
  state.stateProcesses.add(proc);
}

void RemoveStateProcess::redo(const score::DocumentContext& ctx) const
{
  auto& state = m_path.find(ctx);
  state.stateProcesses.remove(m_processId);
}

void RemoveStateProcess::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processUuid << m_processId << m_data;
}

void RemoveStateProcess::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processUuid >> m_processId >> m_data;
}
}
}
