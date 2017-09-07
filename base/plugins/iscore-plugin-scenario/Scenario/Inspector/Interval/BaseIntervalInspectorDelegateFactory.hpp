#pragma once
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>
#include <memory>

namespace Scenario
{
class IntervalInspectorDelegate;
class IntervalModel;

class BaseIntervalInspectorDelegateFactory
    : public IntervalInspectorDelegateFactory
{
  ISCORE_CONCRETE("dee3fedd-4c36-4d2f-8315-448ea593ad46")
public:
  virtual ~BaseIntervalInspectorDelegateFactory();

private:
  std::unique_ptr<IntervalInspectorDelegate>
  make(const IntervalModel& interval) override;

  bool matches(const IntervalModel& interval) const override;
};
}
