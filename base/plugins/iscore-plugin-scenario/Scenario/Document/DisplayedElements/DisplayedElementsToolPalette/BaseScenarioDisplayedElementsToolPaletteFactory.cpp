// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QObject>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include "BaseScenarioDisplayedElementsToolPalette.hpp"
#include "BaseScenarioDisplayedElementsToolPaletteFactory.hpp"
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>

namespace Scenario
{
class ScenarioDocumentPresenter;

std::unique_ptr<GraphicsSceneToolPalette>
BaseScenarioDisplayedElementsToolPaletteFactory::make(
    ScenarioDocumentPresenter& pres, const ConstraintModel& constraint)
{
  return std::make_unique<BaseScenarioDisplayedElementsToolPalette>(pres);
}

bool BaseScenarioDisplayedElementsToolPaletteFactory::matches(
    const ConstraintModel& constraint) const
{
  return dynamic_cast<BaseScenario*>(constraint.parent());
}
}
