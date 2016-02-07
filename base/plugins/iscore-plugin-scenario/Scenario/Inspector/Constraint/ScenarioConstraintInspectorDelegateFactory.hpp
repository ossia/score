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
        ISCORE_CONCRETE_FACTORY_DECL("48765a62-8869-4dbd-ba5d-9a786ce1666f")
    public:
        virtual ~ScenarioConstraintInspectorDelegateFactory();

    private:
        std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) override;

        bool matches(const ConstraintModel& constraint) const override;
};
}
