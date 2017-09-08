#pragma once
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>
#include <memory>

namespace Scenario
{
class IntervalInspectorDelegate;
class IntervalModel;

class ScenarioIntervalInspectorDelegateFactory final
    : public IntervalInspectorDelegateFactory
{
  ISCORE_CONCRETE("48765a62-8869-4dbd-ba5d-9a786ce1666f")
public:
  virtual ~ScenarioIntervalInspectorDelegateFactory();

private:
  std::unique_ptr<IntervalInspectorDelegate>
  make(const IntervalModel& interval) override;

  bool matches(const IntervalModel& interval) const override;
};
}
