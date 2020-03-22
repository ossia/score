// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioCopy.hpp"

#include <Dataflow/Commands/CableHelpers.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/std/Optional.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/ptr_set.hpp>
#include <ossia/detail/thread.hpp>

#include <QJsonArray>
#include <QJsonObject>

#include <vector>
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

// MOVEME
bool verifyAndUpdateIfChildOf(ObjectPath& path, const ObjectPath& parent)
{
  auto parent_n = parent.vec().size();
  auto path_n = path.vec().size();
  if (parent_n >= path_n)
    return false;
  for (std::size_t i = 0; i < parent_n; i++)
  {
    if (!(path.vec()[i] == parent.vec()[i]))
      return false;
  }

  SCORE_ASSERT(parent_n > 1);
  path.vec().erase(path.vec().begin(), path.vec().begin() + parent_n - 1);
  return true;
}

template <typename T>
bool verifyAndUpdateIfChildOf(
    Process::CableData& path,
    const std::vector<Path<T>>& vec)
{
  bool source_ok = false;
  for (const auto& parent : vec)
  {
    if (verifyAndUpdateIfChildOf(
            path.source.unsafePath(), parent.unsafePath()))
    {
      source_ok = true;
      break;
    }
  }
  if (!source_ok)
    return false;

  for (const auto& parent : vec)
  {
    if (verifyAndUpdateIfChildOf(path.sink.unsafePath(), parent.unsafePath()))
    {
      return true;
    }
  }
  // must not happen: the sink is already guaranteed to be a child of an
  // interval since we look for all the inlets
  SCORE_ABORT;
}

template <typename T>
Dataflow::SerializedCables cablesToCopy(
    const std::vector<T*>& array,
    const std::vector<Path<std::remove_const_t<T>>>& siblings,
    const score::DocumentContext& ctx)
{
  // For every cable, if both ends are in one of the elements or child elements
  // currently selected, we copy them.
  // Note: ids / cable paths have to be updated of course.
  Dataflow::SerializedCables copiedCables;
  ossia::ptr_set<Process::Inlet*> ins;
  for (auto itv : array)
  {
    auto child_ins = itv->template findChildren<Process::Inlet*>();
    ins.insert(child_ins.begin(), child_ins.end());
  }

  for (auto inl : ins)
  {
    for (const auto& c_inl : inl->cables())
    {
      if (Process::Cable* cable = c_inl.try_find(ctx))
      {
        auto cd = cable->toCableData();
        if (verifyAndUpdateIfChildOf(cd, siblings))
        {
          copiedCables.push_back({cable->id(), cd});
        }
      }
    }
  }

  return copiedCables;
}

template <typename Scenario_T>
QJsonObject
copySelected(const Scenario_T& sm, CategorisedScenario& cs, QObject* parent)
{
  std::vector<Path<Scenario::IntervalModel>> itv_paths;
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

    auto end_it
        = ossia::find_if(cs.selectedStates, [&](const StateModel* state) {
            return state->id() == interval->endState();
          });
    if (end_it == cs.selectedStates.end())
    {
      cs.selectedStates.push_back(&sm.state(interval->endState()));
    }

    itv_paths.push_back(*interval);
  }

  for (const StateModel* state : cs.selectedStates)
  {
    auto ev_it
        = ossia::find_if(cs.selectedEvents, [&](const EventModel* event) {
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
    auto clone_tn = new TimeSyncModel(
        DataStreamWriter{score::marshall<DataStream>(*tn)}, nullptr);
    auto events = clone_tn->events();
    for (const auto& event : events)
    {
      auto absent
          = ossia::none_of(cs.selectedEvents, [&](const EventModel* ev) {
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
    auto clone_ev = new EventModel(
        DataStreamWriter{score::marshall<DataStream>(*ev)}, nullptr);
    auto states = clone_ev->states();
    for (const auto& state : states)
    {
      auto absent
          = ossia::none_of(cs.selectedStates, [&](const StateModel* st) {
              return st->id() == state;
            });
      if (absent)
        clone_ev->removeState(state);
    }

    copiedEvents.push_back(clone_ev);
  }

  const auto& ctx = score::IDocument::documentContext(*parent);
  std::vector<StateModel*> copiedStates;
  copiedStates.reserve(cs.selectedStates.size());
  for (const StateModel* st : cs.selectedStates)
  {
    auto clone_st = new StateModel(
        DataStreamWriter{score::marshall<DataStream>(*st)}, ctx, parent);

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
  base["Cables"]
      = toJsonValueArray(cablesToCopy(cs.selectedIntervals, itv_paths, ctx));

  for (auto elt : copiedTimeSyncs)
    delete elt;
  for (auto elt : copiedEvents)
    delete elt;
  for (auto elt : copiedStates)
    delete elt;

  return base;
}

QJsonObject copyProcess(const Process::ProcessModel& proc)
{
  const auto& ctx = score::IDocument::documentContext(proc);
  QJsonObject base;
  std::vector<const Process::ProcessModel*> vp{&proc};
  std::vector<Path<Process::ProcessModel>> vpath{proc};
  base["PID"] = ossia::get_pid();
  base["Document"] = toJsonValue(ctx.document.id());
  base["Process"] = toJsonObject(proc);
  base["Cables"] = toJsonValueArray(cablesToCopy(vp, vpath, ctx));
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

QJsonObject copyWholeScenario(const Scenario::ProcessModel& sm)
{
  const auto& ctx = score::IDocument::documentContext(sm);

  auto itvs = sm.intervals.map().as_vec();
  std::vector<Path<Scenario::IntervalModel>> itv_paths;
  itv_paths.reserve(sm.intervals.size());
  for (Scenario::IntervalModel& itv : sm.intervals)
  {
    itv_paths.push_back(itv);
  }

  QJsonObject base;

  base["Intervals"] = toJsonArray(sm.intervals);
  base["Events"] = toJsonArray(sm.events);
  base["TimeNodes"] = toJsonArray(sm.timeSyncs);
  base["States"] = toJsonArray(sm.states);
  base["Cables"] = toJsonValueArray(cablesToCopy(itvs, itv_paths, ctx));
  base["Comments"] = toJsonArray(sm.comments);

  return base;
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

CategorisedScenario::CategorisedScenario() {}

template <typename Vector>
std::vector<const typename Vector::value_type*>
selectedElementsVec(const Vector& in)
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

CategorisedScenario::CategorisedScenario(const Selection& sm)
{
  for (auto elt : sm)
  {
    if (auto st = dynamic_cast<const Scenario::StateModel*>(elt.data()))
      selectedStates.push_back(st);
    else if (
        auto itv = dynamic_cast<const Scenario::IntervalModel*>(elt.data()))
      selectedIntervals.push_back(itv);
    else if (auto ev = dynamic_cast<const Scenario::EventModel*>(elt.data()))
      selectedEvents.push_back(ev);
    else if (
        auto ts = dynamic_cast<const Scenario::TimeSyncModel*>(elt.data()))
      selectedTimeSyncs.push_back(ts);
  }
}

QJsonObject copySelectedElementsToJson(
    ScenarioInterface& si,
    const score::DocumentContext& ctx)
{
  auto si_obj = dynamic_cast<QObject*>(&si);
  if (auto sm = dynamic_cast<const Scenario::ProcessModel*>(&si))
  {
    return copySelectedScenarioElements(*sm);
  }
  else if (
      auto bsm = dynamic_cast<const Scenario::BaseScenarioContainer*>(&si))
  {
    return copySelectedScenarioElements(*bsm, si_obj);
  }
  else
  {
    // Full-view copy
    auto& bem
        = score::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(
            ctx.document);
    if (!bem.baseScenario().selectedChildren().empty())
    {
      return copySelectedScenarioElements(
          bem.baseScenario(), &bem.baseScenario());
    }
  }

  return {};
}
}
