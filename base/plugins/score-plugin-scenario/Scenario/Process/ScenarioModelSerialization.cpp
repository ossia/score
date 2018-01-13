// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <Scenario/Application/ScenarioValidity.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <algorithm>
#include <score/tools/std/Optional.hpp>
#include <sys/types.h>

#include "ScenarioFactory.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score_plugin_scenario_export.h>

namespace Process
{
class ProcessModel;
}
class QObject;
struct VisitorVariant;
template <typename T>
class Reader;
template <typename T>
class Writer;


template <>
void DataStreamReader::read(
    const Scenario::ProcessModel& scenario)
{
  // Ports
  m_stream << *scenario.inlet << *scenario.outlet;

  m_stream << scenario.m_startTimeSyncId;
  m_stream << scenario.m_startEventId;
  m_stream << scenario.m_startStateId;

  // Intervals
  const auto& intervals = scenario.intervals;
  m_stream << (int32_t)intervals.size();

  for (const auto& interval : intervals)
  {
    readFrom(interval);
  }

  // Timenodes
  const auto& timesyncs = scenario.timeSyncs;
  m_stream << (int32_t)timesyncs.size();

  for (const auto& timesync : timesyncs)
  {
    readFrom(timesync);
  }

  // Events
  const auto& events = scenario.events;
  m_stream << (int32_t)events.size();

  for (const auto& event : events)
  {
    readFrom(event);
  }

  // States
  const auto& states = scenario.states;
  m_stream << (int32_t)states.size();

  for (const auto& state : states)
  {
    readFrom(state);
  }

  // Comments
  const auto& comments = scenario.comments;
  m_stream << (int32_t)comments.size();

  for (const auto& cmt : comments)
  {
    readFrom(cmt);
  }

  insertDelimiter();
}


template <>
void DataStreamWriter::write(Scenario::ProcessModel& scenario)
{
  // Ports
  scenario.inlet = Process::make_inlet(*this, &scenario);
  scenario.outlet = Process::make_outlet(*this, &scenario);

  m_stream >> scenario.m_startTimeSyncId;
  m_stream >> scenario.m_startEventId;
  m_stream >> scenario.m_startStateId;

  // Intervals
  int32_t interval_count;
  m_stream >> interval_count;

  for (; interval_count-- > 0;)
  {
    auto interval = new Scenario::IntervalModel{*this, &scenario};
    scenario.intervals.add(interval);
  }

  // Timenodes
  int32_t timesync_count;
  m_stream >> timesync_count;

  for (; timesync_count-- > 0;)
  {
    auto tnmodel = new Scenario::TimeSyncModel{*this, &scenario};
    scenario.timeSyncs.add(tnmodel);
  }

  // Events
  int32_t event_count;
  m_stream >> event_count;

  for (; event_count-- > 0;)
  {
    auto evmodel = new Scenario::EventModel{*this, &scenario};
    scenario.events.add(evmodel);
  }

  // States
  int32_t state_count;
  m_stream >> state_count;

  auto& stack = score::IDocument::documentContext(scenario).commandStack;
  for (; state_count-- > 0;)
  {
    auto stmodel = new Scenario::StateModel{*this, stack, &scenario};
    scenario.states.add(stmodel);
  }

  int32_t cmt_count;
  m_stream >> cmt_count;

  for (; cmt_count-- > 0;)
  {
    auto cmtModel = new Scenario::CommentBlockModel{*this, &scenario};
    scenario.comments.add(cmtModel);
  }

  // Finally, we re-set the intervals before and after the states
  for (const Scenario::IntervalModel& interval : scenario.intervals)
  {
    Scenario::SetPreviousInterval(
        scenario.states.at(interval.endState()), interval);
    Scenario::SetNextInterval(
        scenario.states.at(interval.startState()), interval);
  }

  // Scenario::ScenarioValidityChecker::checkValidity(scenario);
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Scenario::ProcessModel& scenario)
{
  obj["Inlet"] = toJsonObject(*scenario.inlet);
  obj["Outlet"] = toJsonObject(*scenario.outlet);
  obj["StartTimeNodeId"] = toJsonValue(scenario.m_startTimeSyncId);
  obj["StartEventId"] = toJsonValue(scenario.m_startEventId);
  obj["StartStateId"] = toJsonValue(scenario.m_startStateId);

  obj["TimeNodes"] = toJsonArray(scenario.timeSyncs);
  obj["Events"] = toJsonArray(scenario.events);
  obj["States"] = toJsonArray(scenario.states);
  obj["Constraints"] = toJsonArray(scenario.intervals);
  obj["Comments"] = toJsonArray(scenario.comments);
}


template <>
void JSONObjectWriter::write(Scenario::ProcessModel& scenario)
{
  {
    JSONObjectWriter writer{obj["Inlet"].toObject()};
    scenario.inlet = Process::make_inlet(writer, &scenario);
  }
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    scenario.outlet = Process::make_outlet(writer, &scenario);
  }

  scenario.m_startTimeSyncId
      = fromJsonValue<Id<Scenario::TimeSyncModel>>(obj["StartTimeNodeId"]);
  scenario.m_startEventId
      = fromJsonValue<Id<Scenario::EventModel>>(obj["StartEventId"]);
  scenario.m_startStateId
      = fromJsonValue<Id<Scenario::StateModel>>(obj["StartStateId"]);

  const auto& intervals = obj["Constraints"].toArray();
  for (const auto& json_vref : intervals)
  {
    auto interval = new Scenario::IntervalModel{
        JSONObject::Deserializer{json_vref.toObject()}, &scenario};
    scenario.intervals.add(interval);
  }

  const auto& timesyncs = obj["TimeNodes"].toArray();
  for (const auto& json_vref : timesyncs)
  {
    auto tnmodel = new Scenario::TimeSyncModel{
        JSONObject::Deserializer{json_vref.toObject()}, &scenario};

    scenario.timeSyncs.add(tnmodel);
  }

  const auto& events = obj["Events"].toArray();
  for (const auto& json_vref : events)
  {
    auto evmodel = new Scenario::EventModel{
        JSONObject::Deserializer{json_vref.toObject()}, &scenario};

    scenario.events.add(evmodel);
  }

  const auto& comments = obj["Comments"].toArray();
  for (const auto& json_vref : comments)
  {
    auto cmtmodel = new Scenario::CommentBlockModel{
        JSONObject::Deserializer{json_vref.toObject()}, &scenario};

    scenario.comments.add(cmtmodel);
  }

  auto& stack = score::IDocument::documentContext(scenario).commandStack;
  const auto& states = obj["States"].toArray();
  for (const auto& json_vref : states)
  {
    auto stmodel = new Scenario::StateModel{
        JSONObject::Deserializer{json_vref.toObject()}, stack, &scenario};

    scenario.states.add(stmodel);
  }

  // Finally, we re-set the intervals before and after the states
  for (const Scenario::IntervalModel& interval : scenario.intervals)
  {
    Scenario::SetPreviousInterval(
        scenario.states.at(interval.endState()), interval);
    Scenario::SetNextInterval(
        scenario.states.at(interval.startState()), interval);
  }

  // Scenario::ScenarioValidityChecker::checkValidity(scenario);
}
