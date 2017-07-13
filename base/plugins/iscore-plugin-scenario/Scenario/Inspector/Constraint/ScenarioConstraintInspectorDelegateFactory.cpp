// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QObject>
#include <Scenario/Process/ScenarioModel.hpp>

#include "ScenarioConstraintInspectorDelegate.hpp"
#include "ScenarioConstraintInspectorDelegateFactory.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>

namespace Scenario
{
ScenarioConstraintInspectorDelegateFactory::
    ~ScenarioConstraintInspectorDelegateFactory()
    = default;

std::unique_ptr<ConstraintInspectorDelegate>
ScenarioConstraintInspectorDelegateFactory::make(
    const ConstraintModel& constraint)
{
  return std::make_unique<ScenarioConstraintInspectorDelegate>(constraint);
}

bool ScenarioConstraintInspectorDelegateFactory::matches(
    const ConstraintModel& constraint) const
{
  return dynamic_cast<Scenario::ProcessModel*>(constraint.parent());
}
}
