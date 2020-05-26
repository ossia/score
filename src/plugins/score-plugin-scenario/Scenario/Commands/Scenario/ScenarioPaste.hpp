#pragma once
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/tools/IdentifierGeneration.hpp>

namespace Scenario
{
struct ScenarioBeingCopied
{
  ScenarioBeingCopied(
      const rapidjson::Value& obj,
      const Scenario::ProcessModel& scenario,
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
      for (const auto& element : json_arr)
      {
        intervals.emplace_back(new IntervalModel{
            JSONObject::Deserializer{element}, scenario.context(), (QObject*)&scenario});
      }
    }
    {
      const auto& json_arr = obj["TimeNodes"].GetArray();
      timesyncs.reserve(json_arr.Size());
      for (const auto& element : json_arr)
      {
        timesyncs.emplace_back(new TimeSyncModel{JSONObject::Deserializer{element}, nullptr});
      }
    }
    {
      const auto& json_arr = obj["Events"].GetArray();
      events.reserve(json_arr.Size());
      for (const auto& element : json_arr)
      {
        events.emplace_back(new EventModel{JSONObject::Deserializer{element}, nullptr});
      }
    }
    {
      const auto& json_arr = obj["States"].GetArray();
      states.reserve(json_arr.Size());
      for (const auto& element : json_arr)
      {
        states.emplace_back(new StateModel{
            JSONObject::Deserializer{element}, scenario.context(), (QObject*)&scenario});
      }
    }
    {
      const auto& json_arr = obj["Cables"].GetArray();
      cables.reserve(json_arr.Size());
      for (const auto& element : json_arr)
      {
        Process::CableData cd;
        if (element.IsObject() && element.HasMember("ObjectName"))
        {
          cd <<= JsonValue{obj["Data"]};
          cables.emplace_back(std::move(cd));
        }
        else if(element.IsArray())
        {
          cd <<= JsonValue{element.GetArray()[1]};
          cables.emplace_back(cd);
        }
      }
    }

    // We generate identifiers for the forthcoming elements
    interval_ids
        = getStrongIdRange2<IntervalModel>(intervals.size(), scenario.intervals, intervals);
    timesync_ids
        = getStrongIdRange2<TimeSyncModel>(timesyncs.size(), scenario.timeSyncs, timesyncs);
    event_ids = getStrongIdRange2<EventModel>(events.size(), scenario.events, events);
    state_ids = getStrongIdRange2<StateModel>(states.size(), scenario.states, states);
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
}
