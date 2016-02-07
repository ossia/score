#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <QObject>

#include "BaseConstraintInspectorDelegate.hpp"
#include "BaseConstraintInspectorDelegateFactory.hpp"
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>

namespace Scenario
{
BaseConstraintInspectorDelegateFactory::~BaseConstraintInspectorDelegateFactory()
{

}

std::unique_ptr<ConstraintInspectorDelegate> BaseConstraintInspectorDelegateFactory::make(
        const ConstraintModel& constraint)
{
    return std::make_unique<BaseConstraintInspectorDelegate>(constraint);
}

bool BaseConstraintInspectorDelegateFactory::matches(
        const ConstraintModel& constraint) const
{
    return dynamic_cast<BaseScenario*>(constraint.parent());
}

auto BaseConstraintInspectorDelegateFactory::concreteFactoryKey() const -> const ConcreteFactoryKey&
{
    static const ConcreteFactoryKey name{"dee3fedd-4c36-4d2f-8315-448ea593ad46"};
    return name;
}
}
