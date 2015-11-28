#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

class BaseScenarioDisplayedElementsProvider final : public DisplayedElementsProvider
{
    public:
        bool matches(
                const ConstraintModel& cst) const override;
        virtual DisplayedElementsContainer make(
                const ConstraintModel& cst) const override;
};
