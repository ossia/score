#include "Encapsulate.hpp"

#include "DuplicateInterval.hpp"

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QJsonObject>
namespace Scenario
{

double findSlotHeightByProcess(const IntervalModel& itv, const Process::ProcessModel& proc)
{
  for (auto& slot : itv.smallView())
  {
    if (ossia::contains(slot.processes, proc.id()))
      return slot.height;
  }

  return proc.getSlotHeight();
}

void DecapsulateScenario(const ProcessModel& scenar, const score::CommandStackFacade& stack)
{
  auto parent_itv = qobject_cast<Scenario::IntervalModel*>(scenar.parent());
  if (!parent_itv)
    return;

  auto parent_s = qobject_cast<Scenario::ProcessModel*>(parent_itv->parent());
  if (!parent_s)
    return;

  auto parent_s_itv = qobject_cast<Scenario::IntervalModel*>(parent_s->parent());
  if (!parent_s_itv)
    return;

  auto scenar_slot_height = findSlotHeightByProcess(*parent_itv, scenar);
  auto parent_scenar_slot_height = findSlotHeightByProcess(*parent_s_itv, *parent_s);

  double ratio = scenar_slot_height / parent_scenar_slot_height;

  using namespace Command;
  Scenario::Command::Macro disp{new Decapsulate, stack.context()};

  JSONReader r;
  copyWholeScenario(r, scenar);

  auto doc = readJson(r.toByteArray()); // TODO UGH
  disp.pasteElementsAfter(*parent_s, Scenario::startTimeSync(*parent_itv, *parent_s), doc, ratio);

  disp.removeProcess(*parent_itv, scenar.id());
  disp.commit();
}

void EncapsulateInScenario(const ProcessModel& scenar, const score::CommandStackFacade& stack)
{
  using namespace Command;
  Scenario::Command::Macro disp{new Encapsulate, stack.context()};

  CategorisedScenario cat{scenar};
  if (cat.selectedIntervals.empty())
    return;

  JSONReader r;
  copySelectedScenarioElements(r, scenar, cat);

  auto e = EncapsulateElements(disp, cat, scenar);
  if (!e.interval)
    return;

  auto& itv = *e.interval;
  auto& sub_scenar = disp.createProcessInSlot<Scenario::ProcessModel>(itv, {}, {});

  // Resize the slot to fit the existing elements
  disp.resizeSlot(
      itv, SlotPath{itv, 0, Slot::RackView::SmallView}, 100 + (e.bottomY - e.topY) * 400);

  disp.pasteElements(sub_scenar, toValue(r), Scenario::Point{{}, 0.1});

  // Merge inside
  for (TimeSyncModel& sync : sub_scenar.timeSyncs)
  {
    if (&sync != &sub_scenar.startTimeSync() && sync.date() == TimeVal::zero())
    {
      disp.mergeTimeSyncs(sub_scenar, sync.id(), sub_scenar.startTimeSync().id());
      break;
    }
  }

  disp.commit();
}

void Duplicate(const ProcessModel& scenar, const score::CommandStackFacade& stack)
{
  using namespace Command;
  CategorisedScenario cat{scenar};
  if (cat.selectedIntervals.empty())
    return;

  auto cmd = new DuplicateInterval{scenar, *cat.selectedIntervals.front()};
  CommandDispatcher<> d{stack};
  d.submit(cmd);
}

EncapsData EncapsulateElements(
    Scenario::Command::Macro& m,
    CategorisedScenario& cat,
    const ProcessModel& scenar)
{
  using namespace Scenario::Command;

  // Compute bounds of elements to encapsulate
  // We require that at most one constraint has its start state not selected
  // and at most one constraint has its end state not selected

  QPointer<Scenario::StateModel> startState{};
  QPointer<Scenario::StateModel> endState{};
  QPointer<const Scenario::TimeSyncModel> startSync{};
  QPointer<const Scenario::TimeSyncModel> endSync{};

  // auto find_state = [&] (auto id) { return
  // ossia::ptr_find_if(cat.selectedStates, [] (auto st) { return st->id() ==
  // id; }); };
  for (auto itv : cat.selectedIntervals)
  {
    auto& start = scenar.states.at(itv->startState());
    if (!start.selection.get())
    {
      if (startState)
      {
        qDebug("Too much entry states");
        return {};
      }
      else
      {
        startState = &start;
      }
    }

    auto& end = scenar.states.at(itv->endState());
    if (!end.selection.get())
    {
      if (endState)
      {
        qDebug("Too much end states");
        return {};
      }
      else
      {
        endState = &end;
      }
    }
  }
  if (startState)
    startSync = &Scenario::parentTimeSync(*startState, scenar);
  if (endState)
    endSync = &Scenario::parentTimeSync(*endState, scenar);

  TimeVal date, endDate;
  double topY = 1000000000;
  double bottomY = 0;
  {
    using comp_t = StartDateComparator<IntervalModel>;
    using set_t = std::set<const IntervalModel*, comp_t>;

    set_t the_set(comp_t{&scenar});
    the_set.insert(cat.selectedIntervals.begin(), cat.selectedIntervals.end());
    date = (*the_set.begin())->date();
  }
  {
    using comp_t = EndDateComparator;
    using set_t = std::set<const IntervalModel*, comp_t>;

    set_t the_set(comp_t{&scenar});
    the_set.insert(cat.selectedIntervals.begin(), cat.selectedIntervals.end());
    auto last = *the_set.rbegin();
    endDate = last->date() + last->duration.defaultDuration();
  }
  {
    for (auto state : cat.selectedStates)
    {
      if (state->heightPercentage() < topY)
        topY = state->heightPercentage();
      if (state->heightPercentage() > bottomY)
        bottomY = state->heightPercentage();
    }
    for (auto itv : cat.selectedIntervals)
    {
      if (itv->heightPercentage() < topY)
        topY = itv->heightPercentage();
      if (itv->heightPercentage() > bottomY)
        bottomY = itv->heightPercentage();
    }
  }

  // Create and paste elements

  // Remove existing elements
  Selection selection;
  auto insert = [&](auto& vec) {
    for (auto ptr : vec)
      selection.append(ptr);
  };
  insert(cat.selectedIntervals);
  insert(cat.selectedStates);
  insert(cat.selectedEvents);
  insert(cat.selectedTimeSyncs);

  m.removeElements(scenar, selection);

  if (!startState && startSync)
  {
    for (const auto& stid : Scenario::states(*startSync, scenar))
    {
      auto& st = scenar.states.at(stid);
      if (st.nextInterval() == ossia::none)
      {
        startState = &st;
        break;
      }
    }
  }
  if (!endState && endSync)
  {
    for (auto stid : Scenario::states(*endSync, scenar))
    {
      auto& st = scenar.states.at(stid);
      if (st.previousInterval() == ossia::none)
      {
        endState = &st;
        break;
      }
    }
  }

  IntervalModel* model{};
  if (startState && endState)
  {
    model = &m.createInterval(scenar, startState->id(), endState->id());
  }
  else if (startState)
  {
    model = &m.createIntervalAfter(scenar, startState->id(), {endDate, topY});
  }
  else if (endState)
  {
    const auto& [ts, e, s] = m.createDot(scenar, {date, topY});
    model = &m.createInterval(scenar, s.id(), endState->id());
  }
  else
  {
    model = &m.createBox(scenar, date, endDate, topY);
  }

  return {topY, bottomY, model};
}
}
