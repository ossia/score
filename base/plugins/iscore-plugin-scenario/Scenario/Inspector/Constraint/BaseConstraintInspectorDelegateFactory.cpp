#include "BaseConstraintInspectorDelegateFactory.hpp"
#include "BaseConstraintInspectorDelegate.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

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
