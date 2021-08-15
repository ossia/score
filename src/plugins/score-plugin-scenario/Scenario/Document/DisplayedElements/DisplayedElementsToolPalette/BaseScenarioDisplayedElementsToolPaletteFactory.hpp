#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>

#include <memory>

class GraphicsSceneToolPalette;
namespace Scenario
{
class IntervalModel;
class ScenarioDocumentPresenter;

class BaseScenarioDisplayedElementsToolPaletteFactory final
    : public DisplayedElementsToolPaletteFactory
{
  SCORE_CONCRETE("ed0d6e10-1bb8-4ee4-b8e9-7e7d9e306e2b")
public:
  bool matches(const IntervalModel& interval) const override;

  std::unique_ptr<GraphicsSceneToolPalette> make(
      ScenarioDocumentPresenter& pres,
      DisplayedElementsPresenter& presenters,
      const IntervalModel& interval,
      QGraphicsItem* parent) override;
};
}
