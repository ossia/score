#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <qobject.h>

#include "BaseScenarioDisplayedElementsToolPalette.hpp"
#include "BaseScenarioDisplayedElementsToolPaletteFactory.hpp"
#include "iscore/statemachine/GraphicsSceneToolPalette.hpp"

class ScenarioDocumentPresenter;

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
