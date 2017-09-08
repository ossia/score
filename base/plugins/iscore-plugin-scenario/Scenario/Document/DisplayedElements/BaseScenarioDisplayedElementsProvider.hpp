#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>

namespace Scenario
{
class IntervalModel;

class BaseScenarioDisplayedElementsProvider final
    : public DisplayedElementsProvider
{
  ISCORE_CONCRETE("a1db53d5-0dcb-412f-9425-8ffcebca023c")
public:
  bool matches(const IntervalModel& cst) const override;
  DisplayedElementsContainer make(IntervalModel& cst) const override;
  DisplayedElementsPresenterContainer make_presenters(
      const IntervalModel& m,
      const Process::ProcessPresenterContext& ctx,
      QGraphicsItem* view_parent,
      QObject* parent) const override;
};
}
