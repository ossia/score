#pragma once
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <memory>

class ConstraintInspectorDelegate;
class ConstraintModel;

class BaseConstraintInspectorDelegateFactory : public ConstraintInspectorDelegateFactory
{
    public:
        virtual ~BaseConstraintInspectorDelegateFactory();

        std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) override;

        bool matches(const ConstraintModel& constraint) const override;
};
