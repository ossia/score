// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioIntervalInspectorDelegateFactory.hpp"

#include "ScenarioIntervalInspectorDelegate.hpp"

#include <QObject>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

namespace Scenario
{
ScenarioIntervalInspectorDelegateFactory::
    ~ScenarioIntervalInspectorDelegateFactory()
    = default;

std::unique_ptr<IntervalInspectorDelegate>
ScenarioIntervalInspectorDelegateFactory::make(const IntervalModel& interval)
{
  return std::make_unique<ScenarioIntervalInspectorDelegate>(interval);
}

bool ScenarioIntervalInspectorDelegateFactory::matches(
    const IntervalModel& interval) const
{
  return dynamic_cast<Scenario::ProcessModel*>(interval.parent());
}
}
