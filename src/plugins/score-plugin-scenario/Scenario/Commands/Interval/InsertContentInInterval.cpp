// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "InsertContentInInterval.hpp"

#include <Process/ExpandMode.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace Scenario
{
namespace Command
{

InsertContentInInterval::InsertContentInInterval(
    const rapidjson::Value& sourceInterval,
    const IntervalModel& targetInterval,
    ExpandMode mode)
    : m_source{clone(sourceInterval)}, m_target{std::move(targetInterval)}, m_mode{mode}
{
  // Generate new ids for each cloned process.
  const auto& target_processes = targetInterval.processes;
  std::vector<Id<Process::ProcessModel>> curIds;
  m_processIds.reserve(target_processes.size());
  std::transform(
      target_processes.begin(),
      target_processes.end(),
      std::back_inserter(curIds),
      [](const auto& proc) { return proc.id(); });

  auto processes = m_source["Processes"].GetArray();
  for (std::size_t i = 0; i < processes.Size(); i++)
  {
    auto obj = processes[i].GetObject();
    Id<Process::ProcessModel> newId = getStrongId(curIds);
    Id<Process::ProcessModel> oldId = Id<Process::ProcessModel>(obj["id"].GetInt());
    obj["id"] = newId.val();
    processes[i] = std::move(obj);
    m_processIds.insert({oldId, newId});
    curIds.push_back(newId);
  }
  m_source["Processes"] = std::move(processes);
}

void InsertContentInInterval::undo(const score::DocumentContext& ctx) const
{
  auto& trg_interval = m_target.find(ctx);
  // We just have to remove what we added
  // TODO Remove the added slots, etc.

  // Remove the processes
  for (const auto& proc_id : m_processIds)
  {
    RemoveProcess(trg_interval, proc_id.second);
  }

  if (trg_interval.processes.empty())
    trg_interval.setSmallViewVisible(false);
}

void InsertContentInInterval::redo(const score::DocumentContext& ctx) const
{
  auto& pl = ctx.app.components.interfaces<Process::ProcessFactoryList>();
  auto& trg_interval = m_target.find(ctx);
  const auto& json_array = m_source["Processes"].GetArray();

  for (const auto& json_vref : json_array)
  {
    JSONObject::Deserializer deserializer{json_vref};
    auto newproc = deserialize_interface(pl, deserializer, ctx, &trg_interval);
    if (newproc)
    {
      AddProcess(trg_interval, newproc);

      // Resize the processes according to the new interval.
      if (m_mode == ExpandMode::Scale)
      {
        newproc->setParentDuration(ExpandMode::Scale, trg_interval.duration.defaultDuration());
      }
      else if (m_mode == ExpandMode::GrowShrink)
      {
        newproc->setParentDuration(ExpandMode::ForceGrow, trg_interval.duration.defaultDuration());
      }
    }
    else
      SCORE_TODO;
  }

  auto sv_it = m_source.FindMember(score::StringConstant().SmallViewRack);
  if (sv_it != m_source.MemberEnd())
  {
    Rack smallView = JsonValue{sv_it->value}.to<Rack>();
    for (auto& sv : smallView)
    {
      if (sv.frontProcess)
      {
        sv.frontProcess = m_processIds.at(*sv.frontProcess);
      }
      for (auto& proc : sv.processes)
      {
        proc = m_processIds.at(proc);
      }
      trg_interval.addSlot(sv);
    }
  }
  if (json_array.Size() > 0 && !trg_interval.smallViewVisible())
    trg_interval.setSmallViewVisible(true);
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
