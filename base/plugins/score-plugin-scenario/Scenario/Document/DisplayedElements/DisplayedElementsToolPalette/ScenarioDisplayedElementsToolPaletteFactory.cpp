// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QObject>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPalette.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include "ScenarioDisplayedElementsToolPaletteFactory.hpp"
#include <score/document/DocumentInterface.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>

namespace Scenario
{
std::unique_ptr<GraphicsSceneToolPalette>
ScenarioDisplayedElementsToolPaletteFactory::make(
    ScenarioDocumentPresenter& pres, const IntervalModel& interval)
{
  return std::make_unique<ScenarioDisplayedElementsToolPalette>(
      pres.displayedElements, pres, pres.view().baseItem());
}

bool ScenarioDisplayedElementsToolPaletteFactory::matches(
    const IntervalModel& interval) const
{
  return dynamic_cast<Scenario::ProcessModel*>(interval.parent());
}
}
