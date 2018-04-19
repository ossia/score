#pragma once
#include <ossia/detail/algorithms.hpp>

#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <memory>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score_plugin_scenario_export.h>
#include <vector>

namespace Scenario
{
class IntervalModel;

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalInspectorDelegateFactory
    : public score::Interface<IntervalInspectorDelegateFactory>
{
  SCORE_INTERFACE("e9ae0303-b616-4953-b148-88d2dda5ac45")
public:
  virtual ~IntervalInspectorDelegateFactory();
  virtual std::unique_ptr<IntervalInspectorDelegate>
  make(const IntervalModel& interval) = 0;
  virtual bool matches(const IntervalModel& interval) const = 0;
};

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalInspectorDelegateFactoryList final
    : public score::MatchingFactory<IntervalInspectorDelegateFactory>
{
};
}
