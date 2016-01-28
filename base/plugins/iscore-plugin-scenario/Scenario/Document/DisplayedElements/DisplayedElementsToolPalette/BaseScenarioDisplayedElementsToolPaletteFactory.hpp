#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <memory>

class GraphicsSceneToolPalette;
namespace Scenario
{
class ConstraintModel;
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
}
