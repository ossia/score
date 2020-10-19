#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

namespace Scenario
{
class IntervalModel;

class ScenarioDisplayedElementsProvider final : public DisplayedElementsProvider
{
  SCORE_CONCRETE("acc060fe-6aa5-415f-b3f9-d082e6f52ce8")
public:
  bool matches(const IntervalModel& cst) const override;
  DisplayedElementsContainer make(IntervalModel& cst) const override;

  DisplayedElementsPresenterContainer make_presenters(
      ZoomRatio zoom,
      const IntervalModel& m,
      const Process::Context& ctx,
      QGraphicsItem* view_parent,
      QObject* parent) const override;
};

class DefaultDisplayedElementsProvider final : public DisplayedElementsProvider
{
  SCORE_CONCRETE("e998df19-1ce9-4f30-b0bb-1025425bd382")
public:
  bool matches(const IntervalModel& cst) const override;
  DisplayedElementsContainer make(IntervalModel& cst) const override;

  DisplayedElementsPresenterContainer make_presenters(
      ZoomRatio zoom,
      const IntervalModel& m,
      const Process::Context& ctx,
      QGraphicsItem* view_parent,
      QObject* parent) const override;
};
}
