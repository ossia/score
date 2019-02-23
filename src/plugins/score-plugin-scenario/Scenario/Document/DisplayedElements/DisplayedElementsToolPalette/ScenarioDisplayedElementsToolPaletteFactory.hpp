#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>

#include <memory>

class GraphicsSceneToolPalette;

namespace Scenario
{
class IntervalModel;
class ScenarioDocumentPresenter;

class ScenarioDisplayedElementsToolPaletteFactory final
    : public DisplayedElementsToolPaletteFactory
{
  SCORE_CONCRETE("d3cbf795-6e95-49bf-b727-f3a9531cf099")
public:
  bool matches(const IntervalModel& interval) const override;

  std::unique_ptr<GraphicsSceneToolPalette> make(
      ScenarioDocumentPresenter& pres,
      const IntervalModel& interval,
      QGraphicsItem* parent) override;
};
}
