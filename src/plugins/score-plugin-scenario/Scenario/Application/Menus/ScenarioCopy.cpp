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
#include <score/model/EntityMap.hpp>
#include <score/model/EntityMapSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/std/Optional.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/ptr_set.hpp>
#include <ossia/detail/thread.hpp>

#include <vector>
namespace Scenario
{
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
bool verifyAndUpdateIfChildOf(Process::CableData& path, const std::vector<Path<T>>& vec)
{
  bool source_ok = false;
  for (const auto& parent : vec)
  {
    if (verifyAndUpdateIfChildOf(path.source.unsafePath(), parent.unsafePath()))
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
void copySelected(JSONReader& r, const Scenario_T& sm, CategorisedScenario& cs, QObject* parent)
{
  std::vector<Path<Scenario::IntervalModel>> itv_paths;
  for (const IntervalModel* interval : cs.selectedIntervals)
  {
    auto start_it = ossia::find_if(cs.selectedStates, [&](const StateModel* state) {
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

    itv_paths.push_back(*interval);
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
    auto tn_it = ossia::find_if(cs.selectedTimeSyncs, [&](const TimeSyncModel* tn) {
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
      auto absent = ossia::none_of(
          cs.selectedEvents, [&](const EventModel* ev) { return ev->id() == event; });
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
      auto absent = ossia::none_of(
          cs.selectedStates, [&](const StateModel* st) { return st->id() == state; });
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
    auto clone_st
        = new StateModel(DataStreamWriter{score::marshall<DataStream>(*st)}, ctx, parent);

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

  r.obj["Intervals"] = cs.selectedIntervals;
  r.obj["Events"] = copiedEvents;
  r.obj["TimeNodes"] = copiedTimeSyncs;
  r.obj["States"] = copiedStates;
  r.obj["Cables"] = cablesToCopy(cs.selectedIntervals, itv_paths, ctx);

  for (auto elt : copiedTimeSyncs)
    delete elt;
  for (auto elt : copiedEvents)
    delete elt;
  for (auto elt : copiedStates)
    delete elt;
}

void copyProcess(JSONReader& r, const Process::ProcessModel& proc)
{
  const auto& ctx = score::IDocument::documentContext(proc);

  std::vector<const Process::ProcessModel*> vp{&proc};
  std::vector<Path<Process::ProcessModel>> vpath{proc};
  // Object is not created here but in SlotHeader
  r.obj["PID"] = ossia::get_pid();
  r.obj["Document"] = ctx.document.id();
  r.obj["Process"] = proc;
  r.obj["Cables"] = cablesToCopy(vp, vpath, ctx);
}

void copySelectedScenarioElements(
    JSONReader& r,
    const Scenario::ProcessModel& sm,
    CategorisedScenario& cat)
{
  r.stream.StartObject();
  copySelected(r, sm, cat, const_cast<Scenario::ProcessModel*>(&sm));

  r.obj["Comments"] = selectedElements(sm.comments);

  r.stream.EndObject();
}

void copyWholeScenario(JSONReader& r, const Scenario::ProcessModel& sm)
{
  const auto& ctx = score::IDocument::documentContext(sm);

  auto itvs = sm.intervals.map().as_vec();
  std::vector<Path<Scenario::IntervalModel>> itv_paths;
  itv_paths.reserve(sm.intervals.size());
  for (Scenario::IntervalModel& itv : sm.intervals)
  {
    itv_paths.push_back(itv);
  }

  r.stream.StartObject();
  r.obj["Intervals"] = sm.intervals;
  r.obj["Events"] = sm.events;
  r.obj["TimeNodes"] = sm.timeSyncs;
  r.obj["States"] = sm.states;
  r.obj["Cables"] = cablesToCopy(itvs, itv_paths, ctx);
  r.obj["Comments"] = sm.comments;
  r.stream.EndObject();
}

void copySelectedScenarioElements(JSONReader& r, const Scenario::ProcessModel& sm)
{
  CategorisedScenario cat{sm};
  return copySelectedScenarioElements(r, sm, cat);
}

void copySelectedScenarioElements(JSONReader& r, const BaseScenarioContainer& sm, QObject* parent)
{
  CategorisedScenario cat{sm};

  r.stream.StartObject();
  copySelected(r, sm, cat, parent);
  r.stream.EndObject();
}

CategorisedScenario::CategorisedScenario() { }

template <typename Vector>
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

CategorisedScenario::CategorisedScenario(const ScenarioInterface& sm)
{
  for (auto& itv : sm.getIntervals())
    if (itv.selection.get())
      selectedIntervals.push_back(&itv);
  for (auto& itv : sm.getEvents())
    if (itv.selection.get())
      selectedEvents.push_back(&itv);
  for (auto& itv : sm.getStates())
    if (itv.selection.get())
      selectedStates.push_back(&itv);
  for (auto& itv : sm.getTimeSyncs())
    if (itv.selection.get())
      selectedTimeSyncs.push_back(&itv);
}

CategorisedScenario::CategorisedScenario(const Selection& sm)
{
  for (const auto& elt : sm)
  {
    if (auto st = dynamic_cast<const Scenario::StateModel*>(elt.data()))
      selectedStates.push_back(st);
    else if (auto itv = dynamic_cast<const Scenario::IntervalModel*>(elt.data()))
      selectedIntervals.push_back(itv);
    else if (auto ev = dynamic_cast<const Scenario::EventModel*>(elt.data()))
      selectedEvents.push_back(ev);
    else if (auto ts = dynamic_cast<const Scenario::TimeSyncModel*>(elt.data()))
      selectedTimeSyncs.push_back(ts);
  }
}

void copySelectedElementsToJson(
    JSONReader& r,
    ScenarioInterface& si,
    const score::DocumentContext& ctx)
{
  auto si_obj = dynamic_cast<QObject*>(&si);
  if (auto sm = dynamic_cast<const Scenario::ProcessModel*>(&si))
  {
    return copySelectedScenarioElements(r, *sm);
  }
  else if (auto bsm = dynamic_cast<const Scenario::BaseScenarioContainer*>(&si))
  {
    return copySelectedScenarioElements(r, *bsm, si_obj);
  }
  else
  {
    // Full-view copy
    auto& bem = score::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(ctx.document);
    if (!bem.baseScenario().selectedChildren().empty())
    {
      return copySelectedScenarioElements(r, bem.baseScenario(), &bem.baseScenario());
    }
  }
}
}
