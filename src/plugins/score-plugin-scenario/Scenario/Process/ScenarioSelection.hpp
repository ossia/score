#pragma once
#include <score/selection/Selection.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

namespace Scenario
{
inline bool selectionHasScenarioElements(const Selection& sel)
{
  if (sel.empty())
    return false;
  if (qobject_cast<const Scenario::ProcessModel*>(sel.begin()->data()))
    return false;

  return true;
}
}
