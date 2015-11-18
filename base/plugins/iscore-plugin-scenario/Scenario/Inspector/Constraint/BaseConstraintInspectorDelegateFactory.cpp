#include "BaseConstraintInspectorDelegateFactory.hpp"
#include "BaseConstraintInspectorDelegate.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
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
    return constraint.objectName() == "BaseConstraintModel";

}



ScenarioConstraintInspectorDelegateFactory::~ScenarioConstraintInspectorDelegateFactory()
{

}

std::unique_ptr<ConstraintInspectorDelegate> ScenarioConstraintInspectorDelegateFactory::make(
        const ConstraintModel& constraint)
{
    return std::make_unique<ScenarioConstraintInspectorDelegate>(constraint);
}

bool ScenarioConstraintInspectorDelegateFactory::matches(
        const ConstraintModel& constraint) const
{
    return constraint.objectName() == "ConstraintModel";

}
