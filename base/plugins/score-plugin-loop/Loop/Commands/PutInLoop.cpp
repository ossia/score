#include "PutInLoop.hpp"

#include <Loop/LoopProcessModel.hpp>

#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>

namespace Loop
{

void EncapsulateInLoop(const Scenario::ProcessModel& scenar, const score::CommandStackFacade& stack)
{
  using namespace Scenario;
  using namespace Scenario::Command;
  RedoMacroCommandDispatcher<Encapsulate> disp{stack};

  CategorisedScenario cat{scenar};
  if(cat.selectedIntervals.empty())
    return;
  if(cat.selectedIntervals.size() == 1)
  {
    // Just move all the processes in the pattern
    const IntervalModel& source_itv = *cat.selectedIntervals.front();
    auto itv_json = score::marshall<JSONObject>(source_itv);

    auto clear_itv = new ClearInterval{source_itv};
    disp.submitCommand(clear_itv);

    auto create_loop = new AddProcessToInterval{
        source_itv,
                       Metadata<ConcreteKey_k, Loop::ProcessModel>::get(), {}};
    disp.submitCommand(create_loop);

    auto& loop = static_cast<Loop::ProcessModel&>(*source_itv.processes.begin());

    auto cmd = new Scenario::Command::InsertContentInInterval(
          std::move(itv_json), loop.intervals()[0], ExpandMode::Scale);

    disp.submitCommand(cmd);
    disp.commit();
  }
  else
  {
    auto objects = copySelectedScenarioElements(scenar, cat);

    auto e = EncapsulateElements(disp, cat, scenar);
    if(!e.interval)
      return;

    auto& loop_parent_itv = *e.interval;

    auto create_loop = new AddProcessToInterval{
        loop_parent_itv,
        Metadata<ConcreteKey_k, Loop::ProcessModel>::get(), {}};
    disp.submitCommand(create_loop);

    auto& loop = static_cast<Loop::ProcessModel&>(*loop_parent_itv.processes.begin());
    auto& itv = loop.intervals()[0];

    {
      // Add a sub-scenario
      auto create_scenar = new AddProcessToInterval{itv,
          Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}};
      disp.submitCommand(create_scenar);

      auto& sub_scenar = static_cast<Scenario::ProcessModel&>(*itv.processes.begin());
      auto paste = new ScenarioPasteElements(sub_scenar, objects, Scenario::Point{{}, 0.1});
      disp.submitCommand(paste);

      // Merge inside
      for(TimeSyncModel& sync : sub_scenar.timeSyncs) {
        if(&sync != &sub_scenar.startTimeSync() && sync.date() == TimeVal::zero())
        {
          auto mergeStartInside = new Command::MergeTimeSyncs(sub_scenar, sync.id(), sub_scenar.startTimeSync().id());
          disp.submitCommand(mergeStartInside);
          break;
        }
      }
    }

    // Resize the slot to fit the existing elements
    auto resize_slot = new ResizeSlotVertically{
        loop_parent_itv,
        SlotPath{loop_parent_itv, 0, Slot::RackView::SmallView},
        175 + (e.bottomY - e.topY) * 400};
    disp.submitCommand(resize_slot);

    disp.commit();
  }

}

}
