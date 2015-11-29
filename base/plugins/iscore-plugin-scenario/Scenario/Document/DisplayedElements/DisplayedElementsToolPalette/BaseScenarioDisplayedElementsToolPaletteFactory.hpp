#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <memory>

class ConstraintModel;
class GraphicsSceneToolPalette;
class ScenarioDocumentPresenter;

class BaseScenarioDisplayedElementsToolPaletteFactory final : public DisplayedElementsToolPaletteFactory
{
    public:
        bool matches(
                const ConstraintModel& constraint) const override;

        std::unique_ptr<GraphicsSceneToolPalette> make(
                ScenarioDocumentPresenter& pres,
                const ConstraintModel& constraint) override;
};
