// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QObject>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPalette.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include "ScenarioDisplayedElementsToolPaletteFactory.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>

namespace Scenario
{
std::unique_ptr<GraphicsSceneToolPalette>
ScenarioDisplayedElementsToolPaletteFactory::make(
    ScenarioDocumentPresenter& pres, const ConstraintModel& constraint)
{
  return std::make_unique<ScenarioDisplayedElementsToolPalette>(
      pres.displayedElements, pres, pres.view().baseItem());
}

bool ScenarioDisplayedElementsToolPaletteFactory::matches(
    const ConstraintModel& constraint) const
{
  return dynamic_cast<Scenario::ProcessModel*>(constraint.parent());
}
}
