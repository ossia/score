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
}
