#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

namespace Scenario
{
class IntervalModel;
}
namespace Loop
{
class DisplayedElementsProvider final : public Scenario::DisplayedElementsProvider
{
  SCORE_CONCRETE("abf6965a-8e36-472a-a728-50b316c900a4")

public:
  bool matches(const Scenario::IntervalModel& cst) const override;
  Scenario::DisplayedElementsContainer make(Scenario::IntervalModel& cst) const override;

  Scenario::DisplayedElementsPresenterContainer make_presenters(
      const Scenario::IntervalModel& m,
      const Process::Context& ctx,
      QGraphicsItem* view_parent,
      QObject* parent) const override;
};
}
