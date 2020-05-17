// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDisplayedElementsToolPaletteFactory.hpp"

#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPalette.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>

namespace Scenario
{
std::unique_ptr<GraphicsSceneToolPalette> ScenarioDisplayedElementsToolPaletteFactory::make(
    ScenarioDocumentPresenter& pres,
    const IntervalModel& interval,
    QGraphicsItem* parent)
{
  return std::make_unique<ScenarioDisplayedElementsToolPalette>(
      pres.displayedElements, pres, parent);
}

bool ScenarioDisplayedElementsToolPaletteFactory::matches(const IntervalModel& interval) const
{
  return dynamic_cast<Scenario::ProcessModel*>(interval.parent());
}
}
