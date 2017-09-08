#pragma once
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>

namespace Scenario
{
template <typename Fun>
void DoForSelectedIntervals(const iscore::DocumentContext& doc, Fun f)
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
