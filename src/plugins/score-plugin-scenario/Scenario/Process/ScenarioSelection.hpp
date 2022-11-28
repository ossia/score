#pragma once
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/selection/Selection.hpp>

namespace Scenario
{
inline bool selectionHasScenarioElements(const Selection& sel)
{
  if(sel.empty())
    return false;
  if(qobject_cast<const Scenario::ProcessModel*>(sel.begin()->data()))
    return false;

  return true;
}

SCORE_PLUGIN_SCENARIO_EXPORT
std::vector<QObject*>
findByAddress(const score::DocumentContext& ctx, const State::Address& root);
}
