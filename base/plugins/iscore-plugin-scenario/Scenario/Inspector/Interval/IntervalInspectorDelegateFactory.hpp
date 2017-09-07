#pragma once
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <memory>
#include <vector>

#include <ossia/detail/algorithms.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class IntervalModel;

class ISCORE_PLUGIN_SCENARIO_EXPORT IntervalInspectorDelegateFactory
    : public iscore::Interface<IntervalInspectorDelegateFactory>
{
  ISCORE_INTERFACE("e9ae0303-b616-4953-b148-88d2dda5ac45")
public:
  virtual ~IntervalInspectorDelegateFactory();
  virtual std::unique_ptr<IntervalInspectorDelegate>
  make(const IntervalModel& interval) = 0;
  virtual bool matches(const IntervalModel& interval) const = 0;
};

class ISCORE_PLUGIN_SCENARIO_EXPORT IntervalInspectorDelegateFactoryList
    final : public iscore::MatchingFactory<IntervalInspectorDelegateFactory>
{
};
}
