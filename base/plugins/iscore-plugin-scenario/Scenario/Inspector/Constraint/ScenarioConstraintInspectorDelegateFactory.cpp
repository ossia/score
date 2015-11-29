#include <Scenario/Process/ScenarioModel.hpp>
#include <qobject.h>

#include "Scenario/Document/Constraint/ConstraintModel.hpp"
#include "Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp"
#include "ScenarioConstraintInspectorDelegate.hpp"
#include "ScenarioConstraintInspectorDelegateFactory.hpp"

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
    return dynamic_cast<Scenario::ScenarioModel*>(constraint.parent());
}
