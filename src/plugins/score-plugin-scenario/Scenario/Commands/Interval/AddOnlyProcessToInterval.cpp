// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddOnlyProcessToInterval.hpp"

#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/ChangeId.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <vector>

namespace Scenario
{
namespace Command
{
AddOnlyProcessToInterval::AddOnlyProcessToInterval(
    const IntervalModel& cst,
    UuidKey<Process::ProcessModel> process,
    const QString& dat,
    QPointF pos)
    : AddOnlyProcessToInterval{cst, getStrongId(cst.processes), process, dat, pos}
{
}

AddOnlyProcessToInterval::AddOnlyProcessToInterval(
    const IntervalModel& cst,
    Id<Process::ProcessModel> processId,
    UuidKey<Process::ProcessModel> process,
    const QString& dat,
    QPointF pos)
    : m_path{cst}
    , m_processName{process}
    , m_data{dat}
    , m_graphpos{pos}
    , m_createdProcessId{std::move(processId)}
{
  if (m_graphpos == QPointF{})
  {
    m_graphpos = newProcessPosition(cst);
  }
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
  auto fac = ctx.app.interfaces<Process::ProcessFactoryList>().get(m_processName);
  SCORE_ASSERT(fac);
  auto proc = fac->make(
      interval.duration.defaultDuration(), // TODO should maybe be max ?
      m_data,
      m_createdProcessId,
      ctx,
      &interval);

  proc->setPosition(m_graphpos);
  AddProcess(interval, proc);
  return *proc;
}

void AddOnlyProcessToInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processName << m_data << m_graphpos << m_createdProcessId;
}

void AddOnlyProcessToInterval::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processName >> m_data >> m_graphpos >> m_createdProcessId;
}

LoadOnlyProcessInInterval::LoadOnlyProcessInInterval(
    const IntervalModel& cst,
    Id<Process::ProcessModel> processId,
    const rapidjson::Value& dat)
    : m_path{cst}, m_createdProcessId{std::move(processId)}, m_data{clone(dat)}
{
}

void LoadOnlyProcessInInterval::undo(const score::DocumentContext& ctx) const
{
  undo(m_path.find(ctx));
}

void LoadOnlyProcessInInterval::redo(const score::DocumentContext& ctx) const
{
  redo(m_path.find(ctx), ctx);
}

void LoadOnlyProcessInInterval::undo(IntervalModel& interval) const
{
  RemoveProcess(interval, m_createdProcessId);
}

Process::ProcessModel&
LoadOnlyProcessInInterval::redo(IntervalModel& interval, const score::DocumentContext& ctx) const
{
  // Create process model
  JSONWriter r{m_data};
  const auto& obj = r.obj[score::StringConstant().Process];
  auto key = obj[score::StringConstant().uuid].to<UuidKey<Process::ProcessModel>>();

  auto fac = ctx.app.interfaces<Process::ProcessFactoryList>().get(key);
  SCORE_ASSERT(fac);
  // TODO handle missing process
  JSONObject::Deserializer des{obj};
  auto proc = fac->load(des.toVariant(), ctx, &interval);
  const auto ports = proc->findChildren<Process::Port*>();
  for (Process::Port* port : ports)
  {
    while (!port->cables().empty())
    {
      port->removeCable(port->cables().back());
    }
  }
  score::IDocument::changeObjectId(*proc, m_createdProcessId);

  AddProcess(interval, proc);
  return *proc;
}

void LoadOnlyProcessInInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_data << m_createdProcessId;
}

void LoadOnlyProcessInInterval::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_data >> m_createdProcessId;
}

DuplicateOnlyProcessToInterval::DuplicateOnlyProcessToInterval(
    const IntervalModel& cst,
    const Process::ProcessModel& process)
    : DuplicateOnlyProcessToInterval{cst, getStrongId(cst.processes), process}
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

Process::ProcessModel& DuplicateOnlyProcessToInterval::redo(
    IntervalModel& interval,
    const score::DocumentContext& ctx) const
{
  // Create process model
  auto& pl = ctx.app.interfaces<Process::ProcessFactoryList>();
  Process::ProcessModel* proc
      = deserialize_interface(pl, DataStream::Deserializer{m_processData}, ctx, &interval);
  score::IDocument::changeObjectId(*proc, m_createdProcessId);

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
