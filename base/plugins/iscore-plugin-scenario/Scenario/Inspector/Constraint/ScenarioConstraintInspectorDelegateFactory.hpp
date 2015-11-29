#pragma once
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>

class ScenarioConstraintInspectorDelegateFactory : public ConstraintInspectorDelegateFactory
{
    public:
        virtual ~ScenarioConstraintInspectorDelegateFactory();

        std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) override;

        bool matches(const ConstraintModel& constraint) const override;
};
