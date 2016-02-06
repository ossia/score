#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <memory>

class GraphicsSceneToolPalette;

namespace Scenario
{
class ConstraintModel;
class ScenarioDocumentPresenter;

class ScenarioDisplayedElementsToolPaletteFactory final : public DisplayedElementsToolPaletteFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("d3cbf795-6e95-49bf-b727-f3a9531cf099")
    public:
        bool matches(
                const ConstraintModel& constraint) const override;

        std::unique_ptr<GraphicsSceneToolPalette> make(
                ScenarioDocumentPresenter& pres,
                const ConstraintModel& constraint) override;
};
}
