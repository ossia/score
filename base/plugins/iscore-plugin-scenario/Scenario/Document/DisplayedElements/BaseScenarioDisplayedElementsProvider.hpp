#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>

namespace Scenario
{
class ConstraintModel;

class BaseScenarioDisplayedElementsProvider final
    : public DisplayedElementsProvider
{
  ISCORE_CONCRETE("a1db53d5-0dcb-412f-9425-8ffcebca023c")
public:
  bool matches(const ConstraintModel& cst) const override;
  DisplayedElementsContainer make(ConstraintModel& cst) const override;
  DisplayedElementsPresenterContainer make_presenters(
      const ConstraintModel& m,
      const Process::ProcessPresenterContext& ctx,
      QQuickPaintedItem* view_parent,
      QObject* parent) const override;
};
}
