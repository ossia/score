// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioCopy.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <algorithm>
#include <score/tools/std/Optional.hpp>
#include <vector>

#include <ossia/detail/algorithms.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
namespace Scenario
{
template <typename Selected_T>
static auto arrayToJson(Selected_T&& selected)
{
  QJsonArray array;
  if (!selected.empty())
  {
    for (const auto& element : selected)
    {
      array.push_back(score::marshall<JSONObject>(*element));
    }
  }

  return array;
}

template <typename Scenario_T>
QJsonObject copySelected(const Scenario_T& sm, CategorisedScenario& cs, QObject* parent)
{
  for (const IntervalModel* interval : cs.selectedIntervals)
  {
    auto start_it
        = ossia::find_if(cs.selectedStates, [&](const StateModel* state) {
            return state->id() == interval->startState();
          });
    if (start_it == cs.selectedStates.end())
    {
      cs.selectedStates.push_back(&sm.state(interval->startState()));
    }

    auto end_it = ossia::find_if(cs.selectedStates, [&](const StateModel* state) {
      return state->id() == interval->endState();
    });
    if (end_it == cs.selectedStates.end())
    {
      cs.selectedStates.push_back(&sm.state(interval->endState()));
    }
  }

  for (const StateModel* state : cs.selectedStates)
  {
    auto ev_it = ossia::find_if(cs.selectedEvents, [&](const EventModel* event) {
      return state->eventId() == event->id();
    });
    if (ev_it == cs.selectedEvents.end())
    {
      cs.selectedEvents.push_back(&sm.event(state->eventId()));
    }

    // If the previous or next interval is not here, we set it to null in a
    // copy.
  }
  for (const EventModel* event : cs.selectedEvents)
  {
    auto tn_it
        = ossia::find_if(cs.selectedTimeSyncs, [&](const TimeSyncModel* tn) {
            return tn->id() == event->timeSync();
          });
    if (tn_it == cs.selectedTimeSyncs.end())
    {
      cs.selectedTimeSyncs.push_back(&sm.timeSync(event->timeSync()));
    }

    // If some events aren't there, we set them to null in a copy.
  }

  std::vector<TimeSyncModel*> copiedTimeSyncs;
  copiedTimeSyncs.reserve(cs.selectedTimeSyncs.size());
  for (const auto& tn : cs.selectedTimeSyncs)
  {
    auto clone_tn = new TimeSyncModel(DataStreamWriter{score::marshall<DataStream>(*tn)}, nullptr);
    auto events = clone_tn->events();
    for (const auto& event : events)
    {
      auto absent = ossia::none_of(cs.selectedEvents, [&](const EventModel* ev) {
        return ev->id() == event;
      });
      if (absent)
        clone_tn->removeEvent(event);
    }

    copiedTimeSyncs.push_back(clone_tn);
  }

  std::vector<EventModel*> copiedEvents;
  copiedEvents.reserve(cs.selectedEvents.size());
  for (const auto& ev : cs.selectedEvents)
  {
    auto clone_ev = new EventModel(DataStreamWriter{score::marshall<DataStream>(*ev)}, nullptr);
    auto states = clone_ev->states();
    for (const auto& state : states)
    {
      auto absent = ossia::none_of(cs.selectedStates, [&](const StateModel* st) {
        return st->id() == state;
      });
      if (absent)
        clone_ev->removeState(state);
    }

    copiedEvents.push_back(clone_ev);
  }

  std::vector<StateModel*> copiedStates;
  copiedStates.reserve(cs.selectedStates.size());
  auto& stack = score::IDocument::documentContext(*parent).commandStack;
  for (const StateModel* st : cs.selectedStates)
  {
    auto clone_st = new StateModel(DataStreamWriter{score::marshall<DataStream>(*st)}, stack, parent);

    // NOTE : we must not serialize the state with their previous / next
    // interval
    // since they will change once pasted and cause crash at the end of the
    // ctor
    // of StateModel. They are saved in the previous / next state of interval
    // anyway.
    SetNoPreviousInterval(*clone_st);
    SetNoNextInterval(*clone_st);

    copiedStates.push_back(clone_st);
  }

  QJsonObject base;
  base["Intervals"] = arrayToJson(cs.selectedIntervals);
  base["Events"] = arrayToJson(copiedEvents);
  base["TimeNodes"] = arrayToJson(copiedTimeSyncs);
  base["States"] = arrayToJson(copiedStates);

  for (auto elt : copiedTimeSyncs)
    delete elt;
  for (auto elt : copiedEvents)
    delete elt;
  for (auto elt : copiedStates)
    delete elt;

  return base;
}

QJsonObject copySelectedScenarioElements(
    const Scenario::ProcessModel& sm,
    CategorisedScenario& cat)
{
  auto obj = copySelected(sm, cat, const_cast<Scenario::ProcessModel*>(&sm));

  obj["Comments"] = arrayToJson(selectedElements(sm.comments));

  return obj;
}

QJsonObject copySelectedScenarioElements(const Scenario::ProcessModel& sm)
{
  CategorisedScenario cat{sm};
  return copySelectedScenarioElements(sm, cat);
}

QJsonObject
copySelectedScenarioElements(const BaseScenarioContainer& sm, QObject* parent)
{
  CategorisedScenario cat{sm};
  return copySelected(sm, cat, parent);
}

CategorisedScenario::CategorisedScenario()
{

}

template<typename Vector>
std::vector<const typename Vector::value_type*> selectedElementsVec(const Vector& in)
{
  std::vector<const typename Vector::value_type*> out;
  for (const auto& elt : in)
  {
    if (elt.selection.get())
      out.push_back(&elt);
  }

  return out;
}

CategorisedScenario::CategorisedScenario(const ProcessModel& sm)
{
  selectedIntervals = selectedElementsVec(getIntervals(sm));
  selectedEvents = selectedElementsVec(getEvents(sm));
  selectedTimeSyncs = selectedElementsVec(getTimeSyncs(sm));
  selectedStates = selectedElementsVec(getStates(sm));
}


CategorisedScenario::CategorisedScenario(const BaseScenarioContainer& sm)
{
  selectedIntervals = selectedElementsVec(getIntervals(sm));
  selectedEvents = selectedElementsVec(getEvents(sm));
  selectedTimeSyncs = selectedElementsVec(getTimeSyncs(sm));
  selectedStates = selectedElementsVec(getStates(sm));
}

}
