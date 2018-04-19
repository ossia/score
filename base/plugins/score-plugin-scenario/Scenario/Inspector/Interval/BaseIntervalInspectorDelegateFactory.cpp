// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BaseIntervalInspectorDelegateFactory.hpp"

#include "BaseIntervalInspectorDelegate.hpp"

#include <QObject>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>

namespace Scenario
{
BaseIntervalInspectorDelegateFactory::~BaseIntervalInspectorDelegateFactory()
    = default;

std::unique_ptr<IntervalInspectorDelegate>
BaseIntervalInspectorDelegateFactory::make(const IntervalModel& interval)
{
  return std::make_unique<BaseIntervalInspectorDelegate>(interval);
}

bool BaseIntervalInspectorDelegateFactory::matches(
    const IntervalModel& interval) const
{
  return dynamic_cast<BaseScenario*>(interval.parent());
}
}
