// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QObject>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include "BaseConstraintInspectorDelegate.hpp"
#include "BaseConstraintInspectorDelegateFactory.hpp"
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>

namespace Scenario
{
BaseConstraintInspectorDelegateFactory::
    ~BaseConstraintInspectorDelegateFactory()
    = default;

std::unique_ptr<ConstraintInspectorDelegate>
BaseConstraintInspectorDelegateFactory::make(const ConstraintModel& constraint)
{
  return std::make_unique<BaseConstraintInspectorDelegate>(constraint);
}

bool BaseConstraintInspectorDelegateFactory::matches(
    const ConstraintModel& constraint) const
{
  return dynamic_cast<BaseScenario*>(constraint.parent());
}
}
