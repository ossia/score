#include <Scenario/Process/ScenarioModel.hpp>
#include <QObject>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include "ScenarioConstraintInspectorDelegate.hpp"
#include "ScenarioConstraintInspectorDelegateFactory.hpp"

namespace Scenario
{
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

auto ScenarioConstraintInspectorDelegateFactory::concreteFactoryKey() const -> const ConcreteFactoryKey&
{
    static const ConcreteFactoryKey name{"48765a62-8869-4dbd-ba5d-9a786ce1666f"};
    return name;
}

}
