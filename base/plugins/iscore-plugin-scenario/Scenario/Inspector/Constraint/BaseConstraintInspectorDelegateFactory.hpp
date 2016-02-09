#pragma once
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <memory>

namespace Scenario
{
class ConstraintInspectorDelegate;
class ConstraintModel;

class BaseConstraintInspectorDelegateFactory : public ConstraintInspectorDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("dee3fedd-4c36-4d2f-8315-448ea593ad46")
    public:
        virtual ~BaseConstraintInspectorDelegateFactory();

    private:
        std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) override;

        bool matches(const ConstraintModel& constraint) const override;
};
}
