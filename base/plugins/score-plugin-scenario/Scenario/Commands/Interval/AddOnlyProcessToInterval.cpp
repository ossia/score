// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>

#include <algorithm>
#include <score/tools/IdentifierGeneration.hpp>
#include <vector>

#include "AddOnlyProcessToInterval.hpp"
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>

#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/document/DocumentContext.hpp>

namespace Scenario
{
namespace Command
{
AddOnlyProcessToInterval::AddOnlyProcessToInterval(
    const IntervalModel& cst,
    UuidKey<Process::ProcessModel>
        process)
    : AddOnlyProcessToInterval{cst,
                                 getStrongId(cst.processes),
                                 process}
{
}

AddOnlyProcessToInterval::AddOnlyProcessToInterval(
    const IntervalModel& cst,
    Id<Process::ProcessModel>
        processId,
    UuidKey<Process::ProcessModel>
        process)
    : m_path{cst}
    , m_processName{process}
    , m_createdProcessId{std::move(processId)}
{
}

void AddOnlyProcessToInterval::undo(const score::DocumentContext& ctx) const
{
  undo(m_path.find(ctx));
}

void AddOnlyProcessToInterval::redo(const score::DocumentContext& ctx) const
{
  redo(m_path.find(ctx), ctx);
}

void AddOnlyProcessToInterval::undo(IntervalModel& interval) const
{
  RemoveProcess(interval, m_createdProcessId);
}

Process::ProcessModel&
AddOnlyProcessToInterval::redo(IntervalModel& interval, const score::DocumentContext& ctx) const
{
  // Create process model
  auto fac = ctx.app.interfaces<Process::ProcessFactoryList>().get(
      m_processName);
  SCORE_ASSERT(fac);
  auto proc = fac->make(
      interval.duration.defaultDuration(), // TODO should maybe be max ?
      m_createdProcessId,
      &interval);

  AddProcess(interval, proc);
  return *proc;
}

void AddOnlyProcessToInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processName << m_createdProcessId;
}

void AddOnlyProcessToInterval::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processName >> m_createdProcessId;
}




DuplicateOnlyProcessToInterval::DuplicateOnlyProcessToInterval(
    const IntervalModel& cst,
    const Process::ProcessModel& process)
    : DuplicateOnlyProcessToInterval{cst,
                                 getStrongId(cst.processes),
                                 process}
{
}

DuplicateOnlyProcessToInterval::DuplicateOnlyProcessToInterval(
    const IntervalModel& cst,
    Id<Process::ProcessModel> processId,
    const Process::ProcessModel& process)
    : m_path{cst}
    , m_processData{score::marshall<DataStream>(process)}
    , m_createdProcessId{std::move(processId)}
{
}

void DuplicateOnlyProcessToInterval::undo(const score::DocumentContext& ctx) const
{
  undo(m_path.find(ctx));
}

void DuplicateOnlyProcessToInterval::redo(const score::DocumentContext& ctx) const
{
  redo(m_path.find(ctx), ctx);
}

void DuplicateOnlyProcessToInterval::undo(IntervalModel& interval) const
{
  RemoveProcess(interval, m_createdProcessId);
}

Process::ProcessModel&
DuplicateOnlyProcessToInterval::redo(IntervalModel& interval, const score::DocumentContext& ctx) const
{
  // Create process model
  auto& pl = ctx.app.interfaces<Process::ProcessFactoryList>();
  Process::ProcessModel* proc = deserialize_interface(pl, DataStream::Deserializer{m_processData}, &interval);
  proc->setId(m_createdProcessId);

  AddProcess(interval, proc);

  return *proc;
}

void DuplicateOnlyProcessToInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processData << m_createdProcessId;
}

void DuplicateOnlyProcessToInterval::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processData >> m_createdProcessId;
}
}
}
