#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>


namespace Scenario
{
class ConstraintModel;

class ScenarioDisplayedElementsProvider final :
        public DisplayedElementsProvider
{
        ISCORE_CONCRETE_FACTORY_DECL("acc060fe-6aa5-415f-b3f9-d082e6f52ce8")
    public:
        bool matches(
                const ConstraintModel& cst) const override;
        virtual DisplayedElementsContainer make(
                const ConstraintModel& cst) const override;
};
}
