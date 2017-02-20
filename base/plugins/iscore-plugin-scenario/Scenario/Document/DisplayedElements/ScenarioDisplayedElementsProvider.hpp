#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>

namespace Scenario
{
class ConstraintModel;

class ScenarioDisplayedElementsProvider final
    : public DisplayedElementsProvider
{
  ISCORE_CONCRETE("acc060fe-6aa5-415f-b3f9-d082e6f52ce8")
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
