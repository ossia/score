#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>

class BaseScenarioDisplayedElementsToolPaletteFactory final : public DisplayedElementsToolPaletteFactory
{
    public:
        bool matches(
                const ConstraintModel& constraint) const override;

        std::unique_ptr<GraphicsSceneToolPalette> make(
                ScenarioDocumentPresenter& pres,
                const ConstraintModel& constraint) override;
};
