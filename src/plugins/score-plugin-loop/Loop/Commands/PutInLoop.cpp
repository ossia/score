#include "PutInLoop.hpp"

#include <Loop/LoopProcessModel.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/tools/MapCopy.hpp>
namespace Loop
{

void EncapsulateInLoop(
    const Scenario::ProcessModel& scenar,
    const score::CommandStackFacade& stack)
{
  using namespace Scenario;
  using namespace Scenario::Command;
  Scenario::Command::Macro disp{new Encapsulate, stack.context()};

  CategorisedScenario cat{scenar};
  if (cat.selectedIntervals.empty())
    return;
  if (cat.selectedIntervals.size() == 1)
  {
    // Just move all the processes in the pattern
    const IntervalModel& source_itv = *cat.selectedIntervals.front();

    auto& loop = disp.createProcessInSlot<Loop::ProcessModel>(source_itv, {}, {});
    for (auto proc : shallow_copy(source_itv.processes))
    {
      if (proc != &loop)
      {
        disp.moveProcess(source_itv, loop.intervals()[0], proc->id());
      }
    }
    // TODO copy slots so that the processes are in the correct order

    disp.commit();
  }
  else
  {
    auto objects = copySelectedScenarioElements(scenar, cat);

    auto e = EncapsulateElements(disp, cat, scenar);
    if (!e.interval)
      return;

    auto& loop_parent_itv = *e.interval;

    auto& loop
        = disp.createProcessInSlot<Loop::ProcessModel>(loop_parent_itv, {}, {});

    auto& itv = loop.intervals()[0];

    {
      // Add a sub-scenario
      auto& sub_scenar
          = disp.createProcessInSlot<Scenario::ProcessModel>(itv, {}, {});

      disp.pasteElements(sub_scenar, objects, Scenario::Point{{}, 0.1});

      // Merge inside
      for (TimeSyncModel& sync : sub_scenar.timeSyncs)
      {
        if (&sync != &sub_scenar.startTimeSync()
            && sync.date() == TimeVal::zero())
        {
          disp.mergeTimeSyncs(
              sub_scenar, sync.id(), sub_scenar.startTimeSync().id());
          break;
        }
      }
    }

    // Resize the slot to fit the existing elements
    disp.showRack(loop_parent_itv);
    disp.resizeSlot(
        loop_parent_itv,
        SlotPath{loop_parent_itv, 0, Slot::RackView::SmallView},
        175 + (e.bottomY - e.topY) * 400);

    disp.commit();
  }
}
}
