#pragma once
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/tools/IdentifierGeneration.hpp>

#include <QJsonObject>
namespace Scenario
{
struct ScenarioBeingCopied
{
  ScenarioBeingCopied(
      const QJsonObject& obj,
      const Scenario::ProcessModel& scenario,
      const score::DocumentContext& ctx)
  {
    // TODO this is really a bad idea... either they should be properly added,
    // or the json should be modified without including anything in the
    // scenario. Especially their parents aren't coherent (TimeSync must not
    // have a parent because it tries to access the event in the scenario if it
    // has one) We deserialize everything
    {
      const auto json_arr = obj["Intervals"].toArray();
      intervals.reserve(json_arr.size());
      for (const auto& element : json_arr)
      {
        intervals.emplace_back(
            new IntervalModel{JSONObject::Deserializer{element.toObject()}, scenario.context(),
                              (QObject*)&scenario});
      }
    }
    {
      const auto json_arr = obj["TimeNodes"].toArray();
      timesyncs.reserve(json_arr.size());
      for (const auto& element : json_arr)
      {
        timesyncs.emplace_back(new TimeSyncModel{
            JSONObject::Deserializer{element.toObject()}, nullptr});
      }
    }
    {
      const auto json_arr = obj["Events"].toArray();
      events.reserve(json_arr.size());
      for (const auto& element : json_arr)
      {
        events.emplace_back(new EventModel{
            JSONObject::Deserializer{element.toObject()}, nullptr});
      }
    }
    {
      const auto json_arr = obj["States"].toArray();
      states.reserve(json_arr.size());
      for (const auto& element : json_arr)
      {
        states.emplace_back(
            new StateModel{JSONObject::Deserializer{element.toObject()},
                           scenario.context(),
                           (QObject*)&scenario});
      }
    }
    {
      const auto json_arr = obj["Cables"].toArray();
      cables.reserve(json_arr.size());
      for (const auto& element : json_arr)
      {
        auto obj = element.toObject();
        if(obj.contains("ObjectName"))
        {
          auto cd = fromJsonObject<Process::CableData>(obj["Data"].toObject());
          cables.emplace_back(std::move(cd));
        }
        else
        {
          const auto data = fromJsonValue<Process::CableData>(element.toArray()[1]);
          cables.emplace_back(data);
        }
      }
    }

    // We generate identifiers for the forthcoming elements
    interval_ids = getStrongIdRange2<IntervalModel>(
        intervals.size(), scenario.intervals, intervals);
    timesync_ids = getStrongIdRange2<TimeSyncModel>(
        timesyncs.size(), scenario.timeSyncs, timesyncs);
    event_ids = getStrongIdRange2<EventModel>(
        events.size(), scenario.events, events);
    state_ids = getStrongIdRange2<StateModel>(
        states.size(), scenario.states, states);
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
