#include "BaseScenarioDisplayedElementsToolPaletteFactory.hpp"
#include "BaseScenarioDisplayedElementsToolPalette.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

std::unique_ptr<GraphicsSceneToolPalette> BaseScenarioDisplayedElementsToolPaletteFactory::make(
        ScenarioDocumentPresenter& pres,
        const ConstraintModel& constraint)
{
    return std::make_unique<BaseScenarioDisplayedElementsToolPalette>(pres);
}

bool BaseScenarioDisplayedElementsToolPaletteFactory::matches(
        const ConstraintModel& constraint) const
{
    return dynamic_cast<BaseScenario*>(constraint.parent());
}
