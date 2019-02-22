#pragma once
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveSelection.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

namespace Scenario
{
namespace Command
{
template<typename T>
void setupRemoveMacro(
    const Scenario::ProcessModel& scenar,
    Selection sel,
    T& macro)
{
  switch(sel.size())
  {
  case 0:
    return;
  case 1:
  {
    auto obj = sel.at(0);
    if(auto ts = dynamic_cast<const Scenario::TimeSyncModel*>(obj.data()))
    {
      if(ts->events().size() > 1)
      {
        macro.submit(new SplitWholeSync(*ts));
        return;
      }
      else
      {
        /*
          SCORE_ASSERT(ts->events().size() == 1);
          auto ev = scenar.event(ts->events()[0]);
          SCORE_ASSERT()
          */
        return;
      }
    }
    else if(auto ev = dynamic_cast<const Scenario::EventModel*>(obj.data()))
    {
      if(ev->states().size() > 1)
      {
        macro.submit(new SplitWholeEvent(*ev));
      }
      return;
    }
  }

    [[fallthrough]];
  default:
  {
    CategorisedScenario cat{sel};
    for(auto ts : cat.selectedTimeSyncs)
    {
      if(ts->events().size() > 1)
      {
        auto cmd = new SplitWholeSync{*ts};
        macro.submit(cmd);
      }
    }
    for(auto ev : cat.selectedEvents)
    {
      if(ev->states().size() > 1)
      {
        auto cmd = new SplitWholeEvent{*ev};
        macro.submit(cmd);
      }
    }


    sel.clear();
    for(auto itv : cat.selectedIntervals)
      sel.append(itv);
    for(auto st : cat.selectedStates)
      sel.append(st);

    if(sel.size() > 0)
      macro.submit(new RemoveSelection{scenar, sel});
  }
  }
}
}
}
