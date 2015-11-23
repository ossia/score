#pragma once
#include <Scenario/Document/BaseElement/FullViewStateMachines/FullViewStateMachineFactory.hpp>

class FullViewToolPaletteFactory final : public ScenarioToolPaletteFactory
{
    public:
        bool matches(
                const ConstraintModel& constraint) const override;

        std::unique_ptr<GraphicsSceneToolPalette> make(
                const BaseElementPresenter& pres,
                const ConstraintModel& constraint) override;
};
