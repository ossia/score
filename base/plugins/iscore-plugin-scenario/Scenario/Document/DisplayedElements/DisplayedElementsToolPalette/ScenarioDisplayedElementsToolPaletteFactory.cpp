#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPalette.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <QObject>

#include "ScenarioDisplayedElementsToolPaletteFactory.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>


namespace Scenario
{
std::unique_ptr<GraphicsSceneToolPalette> ScenarioDisplayedElementsToolPaletteFactory::make(
        ScenarioDocumentPresenter& pres,
        const ConstraintModel& constraint)
{
    return std::make_unique<ScenarioDisplayedElementsToolPalette>(
                pres.model().displayedElements,
                pres,
                *pres.view().baseItem());
}

bool ScenarioDisplayedElementsToolPaletteFactory::matches(
        const ConstraintModel& constraint) const
{
    return dynamic_cast<Scenario::ScenarioModel*>(constraint.parent());
}
}
