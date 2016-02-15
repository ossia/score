#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>

namespace Scenario
{
class ConstraintModel;

class BaseScenarioDisplayedElementsProvider final :
        public DisplayedElementsProvider
{
        ISCORE_CONCRETE_FACTORY_DECL("a1db53d5-0dcb-412f-9425-8ffcebca023c")
    public:
        bool matches(
                const ConstraintModel& cst) const override;
        virtual DisplayedElementsContainer make(
                const ConstraintModel& cst) const override;
};
}
