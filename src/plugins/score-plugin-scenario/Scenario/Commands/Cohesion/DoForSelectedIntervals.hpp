#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionStack.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

namespace Scenario
{
template <typename Fun>
void DoForSelectedIntervals(const score::DocumentContext& doc, Fun f)
{
  using namespace std;

  // Fetch the selected intervals
  auto selected_intervals = filterSelectionByType<IntervalModel>(
      doc.selectionStack.currentSelection());

  if (selected_intervals.empty())
    return;

  f(selected_intervals, doc.commandStack);
}
}
