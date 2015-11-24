#pragma once
#include <Scenario/Document/BaseElement/FullViewStateMachines/FullViewStateMachineFactory.hpp>

class BaseScenarioToolPaletteFactory final : public ScenarioToolPaletteFactory
{
    public:
        bool matches(
                const ConstraintModel& constraint) const override;

        std::unique_ptr<GraphicsSceneToolPalette> make(
                BaseElementPresenter& pres,
                const ConstraintModel& constraint) override;
};
