#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <memory>

class GraphicsSceneToolPalette;
namespace Scenario
{
class ConstraintModel;
class ScenarioDocumentPresenter;

class BaseScenarioDisplayedElementsToolPaletteFactory final :
        public DisplayedElementsToolPaletteFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("ed0d6e10-1bb8-4ee4-b8e9-7e7d9e306e2b")
    public:
        bool matches(
                const ConstraintModel& constraint) const override;

        std::unique_ptr<GraphicsSceneToolPalette> make(
                ScenarioDocumentPresenter& pres,
                const ConstraintModel& constraint) override;
};
}
