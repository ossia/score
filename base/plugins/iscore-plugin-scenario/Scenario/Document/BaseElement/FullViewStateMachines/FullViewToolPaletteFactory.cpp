#include "FullViewToolPaletteFactory.hpp"
#include <Scenario/Document/BaseElement/FullViewStateMachines/FullViewStateMachine.hpp>

#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
std::unique_ptr<GraphicsSceneToolPalette> FullViewToolPaletteFactory::make(
        BaseElementPresenter& pres,
        const ConstraintModel& constraint)
{
    return std::make_unique<FullViewToolPalette>(
                iscore::IDocument::documentContext(pres.model()),
                pres.model().displayedElements,
                pres,
                *pres.view().baseItem());
}

bool FullViewToolPaletteFactory::matches(
        const ConstraintModel& constraint) const
{
    return dynamic_cast<ScenarioModel*>(constraint.parent());
}
