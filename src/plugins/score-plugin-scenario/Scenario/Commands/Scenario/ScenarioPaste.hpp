#pragma once
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Scenario/PasteAnchors.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

namespace Scenario
{

std::vector<Process::CableData>
cableDataFromCablesJson(const rapidjson::Document::ConstArray& arr);
std::vector<Process::CableData>
cableDataFromCablesJson(const rapidjson::Document::Array& arr);

// e.g. IntervalModel being copied in a Scenario
template <typename CopiedObjects, typename ParentObject>
ossia::flat_map<Id<Process::Cable>, Process::CableData> mapCopiedCables(
    const score::DocumentContext& ctx, std::vector<Process::CableData>& cables,
    std::vector<CopiedObjects*>& intervals,
    const std::vector<Id<CopiedObjects>>& interval_ids, const ParentObject& scenario)
{
  ossia::flat_map<Id<Process::Cable>, Process::CableData> cable_map;

  {
    ossia::hash_map<Id<CopiedObjects>, Id<CopiedObjects>> id_map;
    {
      int i = 0;
      for(CopiedObjects* interval : intervals)
      {
        id_map[interval->id()] = interval_ids[i];
        i++;
      }
    }

    auto& doc = score::IDocument::modelDelegate<ScenarioDocumentModel>(ctx.document);
    auto cable_ids = getStrongIdRange<Process::Cable>(cables.size(), doc.cables);

    int i = 0;
    Path<ParentObject> p{scenario};
    for(Process::CableData& cd : cables)
    {
      auto& source_vec = cd.source.unsafePath().vec();
      auto& sink_vec = cd.sink.unsafePath().vec();
      SCORE_ASSERT(!source_vec.empty());
      SCORE_ASSERT(!sink_vec.empty());
      int32_t source_itv_id = source_vec.front().id();
      int32_t sink_itv_id = sink_vec.front().id();

      for(CopiedObjects* interval : intervals)
      {
        auto id = interval->id().val();
        if(id == source_itv_id)
          source_itv_id = id_map.at(interval->id()).val();
        if(id == sink_itv_id)
          sink_itv_id = id_map.at(interval->id()).val();
      }
      source_vec.front()
          = ObjectIdentifier{source_vec.front().objectName(), source_itv_id};
      sink_vec.front() = ObjectIdentifier{sink_vec.front().objectName(), sink_itv_id};

      source_vec.insert(
          source_vec.begin(), p.unsafePath().vec().begin(), p.unsafePath().vec().end());
      sink_vec.insert(
          sink_vec.begin(), p.unsafePath().vec().begin(), p.unsafePath().vec().end());

      cable_map.insert({cable_ids[i], std::move(cd)});
      i++;
    }

    for(CopiedObjects* interval : intervals)
    {
      const auto ports = interval->template findChildren<Process::Port*>();
      for(Process::Port* port : ports)
      {
        while(!port->cables().empty())
        {
          port->removeCable(port->cables().back());
        }
      }
    }
  }

  return cable_map;
}

struct ScenarioBeingCopied
{
  ScenarioBeingCopied(
      const rapidjson::Value& obj, const Scenario::ProcessModel& scenario,
      const score::DocumentContext& ctx)
  {
    // TODO this is really a bad idea... either they should be properly added,
    // or the json should be modified without including anything in the
    // scenario. Especially their parents aren't coherent (TimeSync must not
    // have a parent because it tries to access the event in the scenario if it
    // has one) We deserialize everything
    {
      const auto& json_arr = obj["Intervals"].GetArray();
      intervals.reserve(json_arr.Size());
      for(const auto& element : json_arr)
      {
        intervals.emplace_back(new IntervalModel{
            JSONObject::Deserializer{element}, scenario.context(), (QObject*)&scenario});
      }
    }
    {
      const auto& json_arr = obj["TimeNodes"].GetArray();
      timesyncs.reserve(json_arr.Size());
      for(const auto& element : json_arr)
      {
        timesyncs.emplace_back(
            new TimeSyncModel{JSONObject::Deserializer{element}, nullptr});
      }
    }
    {
      const auto& json_arr = obj["Events"].GetArray();
      events.reserve(json_arr.Size());
      for(const auto& element : json_arr)
      {
        events.emplace_back(new EventModel{JSONObject::Deserializer{element}, nullptr});
      }
    }
    {
      const auto& json_arr = obj["States"].GetArray();
      states.reserve(json_arr.Size());
      for(const auto& element : json_arr)
      {
        states.emplace_back(new StateModel{
            JSONObject::Deserializer{element}, scenario.context(), (QObject*)&scenario});
      }
    }
    {
      const auto& json_arr = obj["Cables"].GetArray();
      cables = cableDataFromCablesJson(json_arr);
    }

    // We generate identifiers for the forthcoming elements
    interval_ids = getStrongIdRange2<IntervalModel>(
        intervals.size(), scenario.intervals, intervals);
    timesync_ids = getStrongIdRange2<TimeSyncModel>(
        timesyncs.size(), scenario.timeSyncs, timesyncs);
    event_ids = getStrongIdRange2<EventModel>(events.size(), scenario.events, events);
    state_ids = getStrongIdRange2<StateModel>(states.size(), scenario.states, states);

    remapAnchors(obj, scenario, ctx);
  }

  // Retargets anchors to the new identifiers of the pasted elements
  void remapAnchors(
      const rapidjson::Value& obj, const Scenario::ProcessModel& scenario,
      const score::DocumentContext& ctx)
  {
    CopiedPaths paths;
    paths.sameDocument = copiedFromHere(obj, ctx);
    if(auto it = obj.FindMember("Origin"); it != obj.MemberEnd())
    {
      ObjectPath origin;
      origin <<= JsonValue{it->value};
      const auto here = score::IDocument::unsafe_path(scenario);
      auto pair = [&](auto& models, auto& ids) {
        for(std::size_t i = 0; i < models.size(); i++)
        {
          auto from = origin.vec();
          from.emplace_back(models[i]->objectName(), models[i]->id());
          auto to = here.vec();
          to.emplace_back(models[i]->objectName(), ids[i]);
          paths.moved.emplace_back(ObjectPath{std::move(from)}, ObjectPath{std::move(to)});
        }
      };
      pair(intervals, interval_ids);
      pair(timesyncs, timesync_ids);
      pair(events, event_ids);
      pair(states, state_ids);
    }

    for(auto o : intervals)
      remapCopiedAnchors(*o, paths);
    for(auto o : timesyncs)
      remapCopiedAnchors(*o, paths);
    for(auto o : events)
      remapCopiedAnchors(*o, paths);
    for(auto o : states)
      remapCopiedAnchors(*o, paths);
  }

  std::vector<TimeSyncModel*> timesyncs;
  std::vector<IntervalModel*> intervals;
  std::vector<EventModel*> events;
  std::vector<StateModel*> states;
  std::vector<Process::CableData> cables;

  std::vector<Id<IntervalModel>> interval_ids;
  std::vector<Id<TimeSyncModel>> timesync_ids;
  std::vector<Id<EventModel>> event_ids;
  std::vector<Id<StateModel>> state_ids;
};

struct ProcessesBeingCopied
{
  ProcessesBeingCopied(
      const rapidjson::Value::ConstArray& sourceProcesses,
      const Scenario::IntervalModel& parent, const score::DocumentContext& ctx)
  {
    // TODO this is (again) really a bad idea... either they should be properly added,
    // or the json should be modified without including anything in the
    // scenario. Especially their parents aren't coherent (TimeSync must not
    // have a parent because it tries to access the event in the scenario if it
    // has one) We deserialize everything
    {
      auto& pl = ctx.app.interfaces<Process::ProcessFactoryList>();
      const auto& json_arr = sourceProcesses;
      processes.reserve(json_arr.Size());
      int i = 0;
      for(const auto& element : json_arr)
      {
        JSONObject::Deserializer deserializer{element};
        auto proc = deserialize_interface(
            pl, deserializer, ctx, const_cast<Scenario::IntervalModel*>(&parent));
        if(proc)
        {
          processes.emplace_back(proc);
          sources.push_back(i);
        }
        i++;
      }
    }

    // We generate identifiers for the forthcoming elements
    processes_ids = getStrongIdRange2<Process::ProcessModel>(
        processes.size(), parent.processes, processes);
  }

  // "ProcessPaths" holds the original path of each copied process, or
  // "ProcessesParent" the path of the interval they all come from.
  void remapAnchors(
      const rapidjson::Value& copy, const Scenario::IntervalModel& parent,
      const score::DocumentContext& ctx)
  {
    CopiedPaths paths;
    paths.sameDocument = copiedFromHere(copy, ctx);
    if(auto it = copy.FindMember("ProcessPaths");
       it != copy.MemberEnd() && it->value.IsArray())
    {
      const auto& arr = it->value.GetArray();
      const auto here = score::IDocument::unsafe_path(parent);
      for(std::size_t i = 0; i < processes.size(); i++)
      {
        if(sources[i] >= (int)arr.Size())
          continue;
        ObjectPath from;
        from <<= JsonValue{arr[sources[i]]};
        auto to = here.vec();
        to.emplace_back(processes[i]->objectName(), processes_ids[i]);
        paths.moved.emplace_back(std::move(from), ObjectPath{std::move(to)});
      }
    }
    else if(auto it = copy.FindMember("ProcessesParent"); it != copy.MemberEnd())
    {
      ObjectPath parentPath;
      parentPath <<= JsonValue{it->value};
      const auto here = score::IDocument::unsafe_path(parent);
      for(std::size_t i = 0; i < processes.size(); i++)
      {
        auto from = parentPath.vec();
        from.emplace_back(processes[i]->objectName(), processes[i]->id());
        auto to = here.vec();
        to.emplace_back(processes[i]->objectName(), processes_ids[i]);
        paths.moved.emplace_back(ObjectPath{std::move(from)}, ObjectPath{std::move(to)});
      }
    }

    for(auto proc : processes)
      remapCopiedAnchors(*proc, paths);
  }

  std::vector<Process::ProcessModel*> processes;
  std::vector<Id<Process::ProcessModel>> processes_ids;
  //! Index of each process in the copied array
  std::vector<int> sources;
};
}
