#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

namespace Scenario
{
class ConstraintModel;
}
namespace Loop
{
class DisplayedElementsProvider final
    : public Scenario::DisplayedElementsProvider
{
  ISCORE_CONCRETE("abf6965a-8e36-472a-a728-50b316c900a4")

public:
  bool matches(const Scenario::ConstraintModel& cst) const override;
  Scenario::DisplayedElementsContainer
  make(Scenario::ConstraintModel& cst) const override;

  Scenario::DisplayedElementsPresenterContainer make_presenters(
      const Scenario::ConstraintModel& m,
      const Process::ProcessPresenterContext& ctx,
      QQuickPaintedItem* view_parent,
      QObject* parent) const override;
};
}
