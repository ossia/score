#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveSelection.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/ResizeSlotVertically.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeEvents.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <score/command/CommandData.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>

namespace Scenario
{
class IntervalModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT Encapsulate final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), Encapsulate, "Encapsulate")
};
}

struct EncapsData
{
  double topY{}, bottomY{};
  IntervalModel* interval{};
};
template<typename Disp>
EncapsData EncapsulateElements(
    RedoMacroCommandDispatcher<Disp>& disp,
    CategorisedScenario& cat,
    const ProcessModel& scenar)
{
  using namespace Scenario::Command;

  // Compute bounds of elements to encapsulate
  // We require that at most one constraint has its start state not selected and at most one constraint has its end state not selected

  QPointer<Scenario::StateModel> startState{};
  QPointer<Scenario::StateModel> endState{};
  QPointer<const Scenario::TimeSyncModel> startSync{};
  QPointer<const Scenario::TimeSyncModel> endSync{};

  //auto find_state = [&] (auto id) { return ossia::ptr_find_if(cat.selectedStates, [] (auto st) { return st->id() == id; }); };
  for(auto itv : cat.selectedIntervals)
  {
    auto& start = scenar.states.at(itv->startState());
    if(!start.selection.get())
    {
      if(startState) {
        qDebug("Too much entry states");
        return {};
      }
      else {
        startState = &start;
      }
    }

    auto& end = scenar.states.at(itv->endState());
    if(!end.selection.get())
    {
      if(endState) {
        qDebug("Too much end states");
        return {};
      }
      else {
        endState = &end;
      }
    }
  }
  if(startState) startSync = &Scenario::parentTimeSync(*startState, scenar);
  if(endState) endSync = &Scenario::parentTimeSync(*endState, scenar);

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
    for(auto state : cat.selectedStates)
    {
      if(state->heightPercentage() < topY)
        topY = state->heightPercentage();
      if(state->heightPercentage() > bottomY)
        bottomY = state->heightPercentage();
    }
    for(auto itv : cat.selectedIntervals)
    {
      if(itv->heightPercentage() < topY)
        topY = itv->heightPercentage();
      if(itv->heightPercentage() > bottomY)
        bottomY = itv->heightPercentage();
    }
  }

  // Create and paste elements

  // Remove existing elements
  Selection selection;
  auto insert = [&] (auto& vec) { for(auto ptr : vec) selection.append(ptr); };
  insert(cat.selectedIntervals);
  insert(cat.selectedStates);
  insert(cat.selectedEvents);
  insert(cat.selectedTimeSyncs);

  auto rm = new RemoveSelection{scenar, selection};
  disp.submitCommand(rm);

  if(!startState && startSync)
  {
    for(auto stid : Scenario::states(*startSync, scenar)) {
      auto& st = scenar.states.at(stid);
      if(st.nextInterval() == ossia::none)
      {
        startState = &st;
        break;
      }
    }
  }
  if(!endState && endSync)
  {
    for(auto stid : Scenario::states(*endSync, scenar)) {
      auto& st = scenar.states.at(stid);
      if(st.previousInterval() == ossia::none)
      {
        endState = &st;
        break;
      }
    }
  }

  IntervalModel* model{};
  if(startState && endState)
  {
    auto create_itv = new CreateInterval{scenar, startState->id(), endState->id()};
    disp.submitCommand(create_itv);
    model = &scenar.interval(create_itv->createdInterval());
  }
  else if(startState)
  {
    auto create_itv = new CreateInterval_State_Event_TimeSync{scenar, startState->id(), endDate, topY};
    disp.submitCommand(create_itv);
    model = &scenar.interval(create_itv->createdInterval());
  }
  else if(endState)
  {
    auto create_start = new CreateTimeSync_Event_State{scenar, date, topY};
    disp.submitCommand(create_start);
    auto create_itv = new CreateInterval{scenar, create_start->createdState(), endState->id()};
    disp.submitCommand(create_itv);
    model = &scenar.interval(create_itv->createdInterval());
  }
  else
  {
    auto create_start = new CreateTimeSync_Event_State{scenar, date, topY};
    disp.submitCommand(create_start);
    auto create_itv = new CreateInterval_State_Event_TimeSync{scenar, create_start->createdState(), endDate, topY};
    disp.submitCommand(create_itv);
    model = &scenar.interval(create_itv->createdInterval());
  }

  return {topY, bottomY, model};
}

void EncapsulateInScenario(
    const ProcessModel& scenar,
    const score::CommandStackFacade& stack);
void Duplicate(
    const ProcessModel& scenar,
    const score::CommandStackFacade& stack);
}
