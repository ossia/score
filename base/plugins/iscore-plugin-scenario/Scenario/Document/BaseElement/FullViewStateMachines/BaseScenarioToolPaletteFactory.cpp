#include "BaseScenarioToolPaletteFactory.hpp"
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenarioStateMachine.hpp>

#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>

#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
std::unique_ptr<GraphicsSceneToolPalette> BaseScenarioToolPaletteFactory::make(
        const BaseElementPresenter& pres,
        const ConstraintModel& constraint)
{
    return std::make_unique<BaseScenarioToolPalette>(pres);
}

bool BaseScenarioToolPaletteFactory::matches(
        const ConstraintModel& constraint) const
{
    return dynamic_cast<BaseScenario*>(constraint.parent());
}
