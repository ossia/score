#pragma once
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <score/selection/Selection.hpp>

namespace Scenario
{
inline void doStateSelection(Selection& sel, const StateModel& m, const BaseScenario& model)
{
  sel.append(m);
}

inline void doStateSelection(Selection& sel, const StateModel& m, const ProcessModel& model)
{
  sel.append(m);

  // If there's a zero-duration interval we also select it to enable
  // it to be removed ; same for the "previous" state which is at the exact same position
  if(auto& id = m.previousInterval())
  {
    auto& itv = Scenario::previousInterval(m, model);
    if(itv.duration.defaultDuration() == TimeVal::zero() && !itv.graphal())
    {
      sel.append(&itv);
      sel.append(Scenario::startState(itv, model));
    }
  }
}
}
