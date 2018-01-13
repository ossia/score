#include "Encapsulate.hpp"

namespace Scenario
{

void EncapsulateInScenario(
    const ProcessModel& scenar,
    const score::CommandStackFacade& stack)
{
  using namespace Command;
  RedoMacroCommandDispatcher<Encapsulate> disp{stack};

  CategorisedScenario cat{scenar};
  if(cat.selectedIntervals.empty())
    return;

  auto objects = copySelectedScenarioElements(scenar, cat);

  auto e = EncapsulateElements(disp, cat, scenar);
  if(!e.interval)
    return;

  auto& itv = *e.interval;
  auto create_scenar = new AddProcessToInterval{itv,
                       Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), QString{}};
  disp.submitCommand(create_scenar);

  // Resize the slot to fit the existing elements
  auto resize_slot = new ResizeSlotVertically{itv, SlotPath{itv, 0, Slot::RackView::SmallView}, 100 + (e.bottomY - e.topY) * 400};
  disp.submitCommand(resize_slot);

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

  disp.commit();
}

}
