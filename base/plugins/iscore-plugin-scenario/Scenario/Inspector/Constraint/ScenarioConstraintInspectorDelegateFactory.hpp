#pragma once
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <memory>

namespace Scenario
{
class ConstraintInspectorDelegate;
class ConstraintModel;

class ScenarioConstraintInspectorDelegateFactory final :
        public ConstraintInspectorDelegateFactory
{
    public:
        virtual ~ScenarioConstraintInspectorDelegateFactory();

    private:
        std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) override;

        bool matches(const ConstraintModel& constraint) const override;

        const ConcreteFactoryKey& concreteFactoryKey() const override;
};
}
