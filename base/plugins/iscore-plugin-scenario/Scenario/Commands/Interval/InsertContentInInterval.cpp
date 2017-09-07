// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <functional>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "InsertContentInInterval.hpp"
#include <Process/ExpandMode.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/application/ApplicationContext.hpp>

namespace Scenario
{
namespace Command
{

InsertContentInInterval::InsertContentInInterval(
    QJsonObject&& sourceInterval,
    const IntervalModel& targetInterval,
    ExpandMode mode)
    : m_source{std::move(sourceInterval)}
    , m_target{std::move(targetInterval)}
    , m_mode{mode}
{
  // Generate new ids for each cloned process.
  const auto& target_processes = targetInterval.processes;
  std::vector<Id<Process::ProcessModel>> target_processes_ids;
  target_processes_ids.reserve(target_processes.size());
  std::transform(
      target_processes.begin(), target_processes.end(),
      std::back_inserter(target_processes_ids),
      [](const auto& proc) { return proc.id(); });

  for (const auto& proc : m_source["Processes"].toArray())
  {
    auto newId = getStrongId(target_processes_ids);
    m_processIds.insert(
        Id<Process::ProcessModel>(proc.toObject()["id"].toInt()), newId);
    target_processes_ids.push_back(newId);
  }
}

void InsertContentInInterval::undo(const iscore::DocumentContext& ctx) const
{
  auto& trg_interval = m_target.find(ctx);
  // We just have to remove what we added
  // TODO Remove the added slots, etc.

  // Remove the processes
  for (const auto& proc_id : m_processIds)
  {
    RemoveProcess(trg_interval, proc_id);
  }
}

void InsertContentInInterval::redo(const iscore::DocumentContext& ctx) const
{
  auto& trg_interval = m_target.find(ctx);
  IntervalModel src_interval{JSONObject::Deserializer{m_source},
                                 &trg_interval}; // Temporary parent

  std::map<const Process::ProcessModel*, Process::ProcessModel*> processPairs;

  // Clone the processes
  const auto& src_procs = src_interval.processes;
  for (const auto& sourceproc : src_procs)
  {
    auto newproc
        = sourceproc.clone(m_processIds[sourceproc.id()], &trg_interval);

    processPairs.insert(std::make_pair(&sourceproc, newproc));
    AddProcess(trg_interval, newproc);

    // Resize the processes according to the new interval.
    if (m_mode == ExpandMode::Scale)
    {
      newproc->setParentDuration(
          ExpandMode::Scale, trg_interval.duration.defaultDuration());
    }
    else if (m_mode == ExpandMode::GrowShrink)
    {
      newproc->setParentDuration(
          ExpandMode::ForceGrow, trg_interval.duration.defaultDuration());
    }
  }

  // TODO add the new processes to the small view
}

void InsertContentInInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_source << m_target << m_processIds << (int)m_mode;
}

void InsertContentInInterval::deserializeImpl(DataStreamOutput& s)
{
  int mode;
  s >> m_source >> m_target >> m_processIds >> mode;
  m_mode = static_cast<ExpandMode>(mode);
}
}
}
