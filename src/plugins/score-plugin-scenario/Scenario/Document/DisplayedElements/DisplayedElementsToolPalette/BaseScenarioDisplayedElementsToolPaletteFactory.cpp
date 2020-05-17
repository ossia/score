// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BaseScenarioDisplayedElementsToolPaletteFactory.hpp"

#include "BaseScenarioDisplayedElementsToolPalette.hpp"

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>

#include <score/statemachine/GraphicsSceneToolPalette.hpp>

namespace Scenario
{
class ScenarioDocumentPresenter;

std::unique_ptr<GraphicsSceneToolPalette> BaseScenarioDisplayedElementsToolPaletteFactory::make(
    ScenarioDocumentPresenter& pres,
    const IntervalModel& interval,
    QGraphicsItem* parent)
{
  return std::make_unique<BaseScenarioDisplayedElementsToolPalette>(pres);
}

bool BaseScenarioDisplayedElementsToolPaletteFactory::matches(const IntervalModel& interval) const
{
  return dynamic_cast<BaseScenario*>(interval.parent());
}
}
